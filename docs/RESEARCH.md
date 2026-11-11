# Wayfarer: research notes

Target: Ocarina of Time Remake 1.1.1, title `0400486024DEC000`, update v131072, main build ID
`E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348`, running on Lichii (Windows). Single player only.

## Goal

A standalone ExeFS mod (`subsdk8` + IPS) that opens a menu on **R3**:

- God Mode (keep current hearts full, never raise the maximum)
- Infinite Rupees, Items, Magic: **top up to the current capacity only**, never above it
  (30/30 bombs, not 99/30; wallet size, not 999)
- Teleport to key places (new)

## Addresses

Guest address = file offset in the decompressed `main` + `0x80020000` (the Hookshot recompiler's convention).
Use file offsets in the mod. Derived by matching Hookshot's function table against `main` prologues.

| What | main offset | Notes |
|---|---|---|
| Event manager slot | `0x4C7E090` | holder -> EventMgr; `+104` non-zero = event playing |
| GameDataMgr slot | `0x4C7E180` | holder -> GameDataMgr |
| GameData bool getter | `0x20D9C0` | `(mgr, out, hash)`; hash `0xEF2B60C2` Sequence_IsLoadingBanc, `0xCEA220D3` Sequence_IsChildhood |
| GameData string getter | `0xCB0EA0` | hash `0x1D6189DA` CurrentBanc, `0xEE27BF71` ReservedNextBanc |
| Make event arg | `0xD79F10` | `x0 = arg buffer` (0x1B20 bytes on stack) |
| Request event | `0xD7A820` | `x0 = EventMgr, x1 = arg` |
| Set starter link | `0x2BC440` | `x0 = arg+0x1950, x1 = actor, x2 = 0` |
| Release link | `0x20B6F0` | cleanup for arg+0x1950 and arg+0x1968 after storing vtable `0x4974B20` |
| Pouch count by name | `0x5963E0` | `x0 = *0x4E78E28 manager, x1 = &name, x2 = 0, x3 = 0` |
| Pouch add/remove | `0x28B4A70` | see Hookshot `oot_pouch_change` |
| Life CAS (God Mode) | `0x49DDF8` | patched by rc1 and by Hookshot's `nx2/oot_patches.py` |

## Teleport algorithm (from Hookshot `host/hos_main.c`, `oot_teleport`, GPL-3.0)

1. Refuse if a request was made in the last 8 s, an event is playing, or `Sequence_IsLoadingBanc` is true.
2. Read `Sequence_IsChildhood` and choose the child or adult map from the destination table.
3. Build an event request named `Dm_SY_BancChangeGo` with args `NextBanc`, `NextStartPos`, `BootEventName`
   (strings, pointers must stay valid) and `IsCheckAir` (bool, false), then set the starter actor and call request.
4. Release the two actor links.

31 destinations are in Hookshot `host/hos_main.c` lines 569-632.

## Mod structure (from rc1 `subsdk8` and its IPS)

- **IPS offsets are memory offsets + 0x100** (Atmosphere adds the NSO header). Every hook-site offset listed
  below and in the IPS files is an IPS offset; subtract 0x100 to index the flat `main` image. (The Hookshot
  function addresses in the table above are memory offsets already.) Verified: `cas w11,w12,[x13]` is at
  memory 0x49DCF8 (IPS 0x49DDF8).
- The IPS patches 17 sites in `main`. Each jumps to a trampoline in a code cave at `0x3C46380+` that loads a
  function pointer from a bridge table at `0x4E9F800+` (8-byte slots) and branches to it when non-null,
  otherwise runs the original instruction.
- rc1 module init (DT_INIT at text 0xE0): checks the title id with `svc 0x29`, finds `main` by walking
  `QueryMemory` downward for the RX region of size 0x3C47000 whose word at +4 is 0x3C4705C, checks the bridge
  table is empty, then fills slots. The module's `ret` at text 0 and MOD0 offset at text 4 are the NSO entry.
- Slot map (IPS site -> slot): npad readers 34695F8/3469624/3469650/346976C/3469798 -> 0x800/0x808/0x810/
  0x818/0x820 (each `bl`s one of five `nn::hid` pad-state functions at memory 0x3C427F0/810/820/800/830;
  signature `long fn(State *out, int count, const u32 *npad_id)`, buttons u64 at `out+8`, R3 = StickR = bit 5);
  render hook 346C0F0/346C1D4 -> 0x828; scene reset 32C1EB0/32D8600/32BBDE0 -> 0x830/0x838/0x840;
  per-frame actor update 49D9C0 -> 0x848 (finds Link by the name "Player"); damage 2F8E860 -> 0x850;
  movement speed 934A90/280FC44 -> 0x858/0x860, 27CD040/27D08E4 -> 0x868; life CAS 49DDF8 -> 0x870 (a data
  slot holding the life pointer, compared in the trampoline).
- rc1 `subsdk8`: text 0x3B7C+, a ~36 MB bss used as an overlay staging buffer, NVN function lookups
  (`nvnQueuePresentTexture`, `nvnWindowInitialize`, ...), menu state struct in bss, rows in a label table.
- rc1 hook sites (main offsets): `49D9C0, 49DDF8, 934A90, 27CD040, 27D08E4, 280FC44, 2F8E860, 32BBDE0,
  32C1EB0, 32D8600, 34695F8, 3469624, 3469650, 346976C, 3469798, 346C0F0, 346C1D4`.
  Their individual purposes still need to be decoded.

## Stage 1 result (verified in the emulator log)

The module loads, the IPS applies, the five npad hooks fire: `[wayfarer] R3 pressed` is logged.
Lessons: the NSO header's bss size counts from the unaligned end of data; MOD0 needs a runtime
module object offset pointing at reserved bss (rtld writes there). Lichii's log is
`%APPDATA%\fritchii\log\eden_log.txt` (previous run in `eden_log.txt.old.txt`); the module's
`svc 0x27` lines show up as `Debug.Emulated ... OutputDebugString`.

## Stage 2 notes (drawing)

- The game loads NVN through `main` 0x129E290 (IPS 0x129E390): `fn(x0, x1 = getProcAddress)` resolves NVN
  functions by name and stores them in a pointer table around `0x4C8D000 + 0xBE8...`. The call sites
  at IPS 346C0F0/346C1D4 are the hook point (bridge slot 0x828): rc1 lets the loader run, then
  swaps entries in that table for its own wrappers (present, memory pool, command buffer).
- Plan: wrap present; on a frame where the menu is open, draw the menu on the CPU (bitmap font,
  rectangles) into a staging buffer and copy it onto the swapchain texture with an NVN command
  buffer before presenting. Avoids needing compiled shaders.

## Tooling

- `tools/nso.py` loads an NSO (LZ4 handled), `tools/pe.py` parses Windows PE files.
- Zig 0.17.1 builds ARM64 (`zig cc -target aarch64-freestanding`).
- The update `main` in the raw dump is packed (`ZBIC`); use an unpacked `main` (Hookshot's `.hookshot_exefs`).

## Licence

Teleport logic derives from Hookshot (GPL-3.0). Wayfarer is therefore GPL-3.0.

## Stage 2 implementation (untested until the user runs it)

- Loader hook (slot 0x828) runs the game's NVN loader, resolves 31 NVN procs, checks that the game's four
  function slots (`main+0x4C8DDE8+{0,0x20,0xA8,0xB0}`, each a pointer to a slot) still hold the resolved procs,
  then swaps in wrappers for present, window-builder-set-textures, window-initialize and window-finalize.
- The pool, syncs and command buffers are created lazily the first time the menu draws.
- On a present with the menu open: copy the panel rectangle of the swapchain texture to a pool buffer, wait for
  the GPU, blend the panel on the CPU (bilinear-scaled Aileron font, CC0), copy back, then present.
- Input: the npad hooks feed button edges to the menu (R3 toggles, D-pad moves, A toggles, B closes) and zero
  buttons and sticks in the returned samples while the menu is open. The world is not paused yet.
- Formats handled: 37 and 56 (RGBA), 55 (BGRA). Anything else logs "unsupported" and draws nothing.

## Stage 3: how the cheats work (decoded from rc1, untested until run)

- Per-frame hook on the player actor update (IPS 0x49D9C0, slot 0x848). Link is recognised by walking
  owner+0x18 (vtable main+0x4B25E18) -> +0x270 -> +0x320 (back pointer, vtable main+0x4B4C460 at -0x1D50) and the
  name "Player" at (component-0x1D40)+0x58. Life word = [owner+0x1C10]+8, max = [owner+0x1C18]+8 (quarter hearts, <= 0x50).
- God Mode: refill life to max each frame, plus the life CAS trampoline (IPS 0x49DDF8, slot 0x870 = address of the life
  word) which never lets a write lower it.
- Everything else every ~30 frames, only when no event is playing and Sequence_IsLoadingBanc is false:
  rupees = game-data getter 0x17B3B0 / setter 0x9B39E0 with ctx = pouch manager + 0x3BC0, hashes wallet max 0x39E8A20D,
  rupees 0xCBCA2F4D; magic = 0xA676A0 (max) / 0xA67C30 (current), setter hash 0xC995BBB8;
  items = 0x5963E0 (count), 0x595CF0 (capacity of the current pouch), 0x110D570 (add) for Arrow, Bomb, DekuSeed,
  DekuNut, DekuStick, Bombchu (age-filtered; bow/bomb bag/slingshot ownership decides arrows/bombs/seeds).
- rc1 data tables use R_AARCH64_RELATIVE relocations (a real .rela.dyn); Wayfarer has none, so it avoids pointer tables.

## Stage 4: teleport (v261111.3, untested until the user runs it)

- `tools/gendest.py` turns Hookshot's destination table into `src/destinations.h` (two string blobs, no pointer
  tables: names, then six strings per place). Menu: Teleport row opens a scrolling list (Up/Down, Left/Right jump 7,
  A go, B back); A queues `g_tp_req`, closes the menu.
- The request is served on the game thread inside the player-actor hook (`do_teleport` in `cheats.h`), following
  Hookshot's `oot_teleport`: refuse within 8 s of the last one, while an event plays, or while a map loads; pick
  the child or adult map from `Sequence_IsChildhood` (hash 0xcea220d3, getter 0x20d9c0); build the
  `Dm_SY_BancChangeGo` event argument in a static 0x1b20-byte buffer (strings copied to static buffers, since the
  request does not copy them), set the starter actor (Link), request it, release the two actor links.
- Magic lesson: the HUD's magic is a component of Link's actor (`0x944e50(actor)`, vtable main+0x4b4dfc0, shown value
  at +0x1bbc, busy counter at +0x1b90, refill `0x2e80f80(component)`); the data-store pair (hashes 0x39e8a20d max,
  0xcbca2f4d current) alone does not move the meter. The pair read as "wallet" with `0xa676a0/0xa67c30` is rupees.
