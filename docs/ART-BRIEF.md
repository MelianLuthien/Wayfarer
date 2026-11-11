# Wayfarer menu: art brief

For whoever creates the graphics (another AI or an artist). Wayfarer is a cheat menu drawn on top of
*The Legend of Zelda: Ocarina of Time* (2026 remake). It must look as if it belongs in the game's own pause menu.
All art must be **original**: take the mood from the game, but do not trace or copy Nintendo artwork, and do not use
any logo or trademark (no Triforce, no game logo).

## 1. Style

- **Mood:** the remake's pause menu. Dark carved wood, warm gold-brown trim, ornate curling scroll corners,
  aged parchment panels, cream lettering. Handcrafted and slightly storybook, never glossy or sci-fi.
- **Palette** (use as a guide, stay inside this family):
  deep wood `#2B1E13`, lighter wood `#4A3522`, trim gold `#D9BD6A`, dark gold `#A8893F`, parchment `#D8C49A` to
  `#C4AD7C`, parchment ink `#2A1D12`, cream `#F3EAD2`, soft green for "on" `#8FD36B`, selection glow white.
- **Light:** from the top left, soft, subtle bevels. No heavy drop shadows, no neon, no gradients that band.
- **Icons:** one coherent family. Bold readable silhouette, 2 to 3 tones of gold-brown on a transparent
  background, thin dark outline. They must stay readable at 32 px on screen. No text inside icons.
- **No text baked into any image.** The menu draws its own text.

## 2. Technical rules

- **Format:** PNG, 32-bit RGBA, sRGB, transparent background, straight (non-premultiplied) alpha, no embedded
  colour profile quirks.
- **Scale:** everything is specified in "units". **Deliver at 4 pixels per unit.** (If 4x is not possible, 2x is the
  minimum.) The menu is scaled by the screen height, so crisp edges matter.
- **Nothing may touch the image border** (leave 2 units of transparent padding), except the stretchable
  pieces marked "tile" which must repeat seamlessly.
- **Panels are 9-slice:** corners stay fixed, the edges and middle stretch or tile. Deliver each as one image
  with the corner size given.
- Keep every file under 300 KB. Name files exactly as listed. One asset per file.
- Provide a **contact sheet** (all assets on one dark and one light background) as well, for checking.

## 3. Frames and panels (9-slice)

| File | Size (units) | Corner (units) | What it is |
|---|---|---|---|
| `frame_main.png` | 200 x 160 | 56 | The whole menu's outer frame. Carved dark wood with a gold inner trim line, curling scroll flourishes in all four corners. The centre is solid dark wood `#2B1E13` (a subtle wood-grain is fine). The edges must tile. |
| `header_bar.png` | 200 x 44 | 24 left/right | The top bar holding the tabs. Slightly lighter wood, a gold line along the bottom, with a gentle swooping scroll curve at both ends (as in the game's pause menu). Stretches horizontally. |
| `footer_bar.png` | 200 x 28 | 12 left/right | The bottom hint bar. Darker wood with a thin gold line along the top. |
| `row_normal.png` | 120 x 22 | 8 | A list row button: warm brown wood with a thin lighter bevel, rounded corners. |
| `row_selected.png` | 120 x 22 | 8 | Same shape, brighter, with a soft white glow and a crisp white outline (the game's selected look). The glow must fit inside the file. |
| `row_pressed.png` | 120 x 22 | 8 | Same shape, slightly darker and inset (for the instant a button is pressed). |
| `parchment_panel.png` | 120 x 120 | 20 | The info panel on the right: aged parchment with a darker brown torn-edge or stitched border. Subtle paper fibre texture, not busy. |
| `dialog_frame.png` | 160 x 100 | 28 | Warning pop-up: same family as the main frame but smaller, with a gold border and darker centre. |
| `chip_normal.png`, `chip_active.png` | 60 x 14 | 7 | Small pill buttons for section filters. Normal: dark wood with a brown edge. Active: gold filled. |

## 4. Controls

| File | Size (units) | What it is |
|---|---|---|
| `switch_off.png`, `switch_on.png` | 22 x 11 | Toggle switch. Off: dark groove, dull brass knob on the left. On: green-lit groove, bright green-gold knob on the right. |
| `slider_track.png` | 60 x 6 (9-slice, 3 units) | Thin carved groove for the time slider. |
| `slider_fill.png` | 60 x 6 (9-slice, 3 units) | The lit gold part of that groove. |
| `slider_knob.png` | 12 x 12 | Round gold knob with a small highlight. |
| `slot_empty.png` | 28 x 28 | Round item slot, dark wood bowl, thin brown rim. Used for items Link does not have. |
| `slot_owned.png` | 28 x 28 | Same slot with a gold rim and a faint warm inner glow. |
| `slot_selected.png` | 32 x 32 | A white glowing ring (transparent middle) drawn on top of a slot. |
| `badge_count.png` | 14 x 8 (9-slice, 4) | Small cream capsule that shows a quantity ("20") at a slot's lower right. |
| `arrow_select.png` | 8 x 10 | The little gold pointer shown beside the selected row. |
| `arrow_up.png`, `arrow_down.png` | 10 x 6 | Scroll indicators, gold. |
| `lock.png`, `check.png` | 12 x 12 | A small padlock (cream) and a green-gold tick. |
| `divider.png` | 120 x 4 (tile horizontally) | A thin gold line with a tiny diamond ornament in the middle. |

## 5. Button glyphs for the hint bar (round, cream, readable at 10 units)

Each is a **cream disc with a dark brown letter or symbol**, 14 x 14 units (the shoulder buttons are 20 x 12
rounded rectangles). The letters are part of the picture.

`btn_a.png`, `btn_b.png`, `btn_x.png`, `btn_y.png`, `btn_l.png`, `btn_r.png`, `btn_zl.png`, `btn_zr.png`,
`btn_plus.png`, `btn_minus.png`, `btn_r3.png` (a stick with a click mark), `dpad_updown.png`, `dpad_leftright.png`,
`dpad_all.png`.

## 6. Tab and row icons (gold on transparent, 20 x 20 units)

- **Tabs:** `tab_player.png` (a hero silhouette or a heart in a ring), `tab_items.png` (a satchel),
  `tab_world.png` (a small globe or sun and moon), `tab_teleport.png` (a map pin on a swirl).
- **Rows:** `ico_god.png` (heart with a shield), `ico_rupee.png` (a green gem), `ico_magic.png` (a green potion
  or flame in a bottle), `ico_items.png` (arrows and a bomb), `ico_flight.png` (a feather), `ico_age.png`
  (an hourglass), `ico_time.png` (a sun and moon clock), `ico_camera.png` (an eye with a frame), `ico_mark.png` (a
  flag), `ico_return.png` (a curved arrow to a flag), `ico_info.png` (a small "i" scroll).
- **Age tags:** `age_child.png`, `age_adult.png` (14 x 14 units; small round emblems, child = leaf, adult = sword).

## 7. Item icons (the inventory editor)

Each **48 x 48 units**, centred, drawn in the same family. Show the item itself, large, with a thin dark outline.
They are drawn in full colour (gold-brown plus the item's own colours); the menu darkens them itself when the
item is not owned. About 90 in six groups:

- **Equipment (11):** `eq_sword_kokiri`, `eq_sword_master`, `eq_sword_giant`, `eq_sword_biggoron`,
  `eq_shield_deku`, `eq_shield_hylian`, `eq_shield_mirror`, `eq_tunic_goron`, `eq_tunic_zora`,
  `eq_boots_iron`, `eq_boots_hover`.
- **Items (23):** `it_deku_stick`, `it_deku_nut`, `it_bomb`, `it_bombchu`, `it_slingshot`, `it_deku_seed`,
  `it_bow`, `it_arrow`, `it_arrow_fire`, `it_arrow_ice`, `it_arrow_light`, `it_dins_fire`, `it_farores_wind`,
  `it_nayrus_love`, `it_ocarina_fairy`, `it_ocarina_time`, `it_hookshot`, `it_longshot`, `it_boomerang`,
  `it_lens_truth`, `it_magic_bean`, `it_hammer`, `it_bottle`.
- **Masks (8):** `mk_keaton`, `mk_skull`, `mk_spooky`, `mk_bunny`, `mk_goron`, `mk_zora`, `mk_gerudo`,
  `mk_truth`.
- **Upgrades (19):** `up_gauntlet_goron`, `up_gauntlet_silver`, `up_gauntlet_gold`, `up_scale_silver`,
  `up_scale_gold`, `up_bombbag_1`, `up_bombbag_2`, `up_bombbag_3`, `up_quiver_1`, `up_quiver_2`,
  `up_quiver_3`, `up_bulletbag_1`, `up_bulletbag_2`, `up_bulletbag_3`, `up_wallet_2`, `up_wallet_3`,
  `up_magic_1`, `up_magic_2`, `up_defense`.
- **Quest (28):** `qs_stone_forest`, `qs_stone_fire`, `qs_stone_water`, `qs_medal_forest`, `qs_medal_fire`,
  `qs_medal_water`, `qs_medal_shadow`, `qs_medal_spirit`, `qs_medal_light`, and 12 songs
  `qs_song_zelda`, `qs_song_epona`, `qs_song_saria`, `qs_song_sun`, `qs_song_time`, `qs_song_storms`,
  `qs_song_forest`, `qs_song_fire`, `qs_song_water`, `qs_song_shadow`, `qs_song_spirit`, `qs_song_light`
  (each a music-note or staff motif in the colour of its element), then `qs_agony_stone`, `qs_gerudo_card`,
  `qs_skulltula_token`.

Medallions are ring-shaped in the element colour (green, red, blue, purple, orange, yellow); the three stones are
the green emerald, red ruby and blue sapphire; each icon must be recognisable at a glance by colour and shape
alone.

## 8. Branding

- `logo_wayfarer.png` (160 x 32 units): the word **Wayfarer** in a hand-lettered fantasy serif, cream on
  transparent, with a small compass-rose or boot-and-feather emblem to the left. No Nintendo logo shapes.
- `emblem.png` (32 x 32): the emblem alone, for the title bar.
- `bg_vignette.png` (256 x 144, stretch): a very soft dark vignette in the corners, 0 to 60 % black, for dimming the
  game behind the menu.

## 9. Fonts (not art, but needed)

Pick two **free-to-embed** fonts (SIL Open Font Licence or similar) and send the font files with their licences:
a decorative serif for titles (for example Cinzel) and a clean readable humanist sans or serif for body text
(for example Alegreya Sans or Lora). They must contain Latin plus Portuguese accents (á é í ó ú â ê ô ã õ ç).

## 10. Delivery checklist

- One PNG per name above, in a folder `wayfarer-art/` (subfolders: `frames`, `controls`, `buttons`, `icons`,
  `items`, `brand`).
- A contact sheet and a short README listing the exact pixel size of each file.
- Written confirmation that the work is original or AI-generated for this project, with the tool used, so the
  licence can be recorded in the repository.
