# Wayfarer menu: art brief

Wayfarer is a cheat menu drawn on top of *The Legend of Zelda: Ocarina of Time* (2026 remake). It must look as if it
belongs in the game's own pause menu. All art must be **original**: take the mood from the game, but do not trace
or copy Nintendo artwork and do not use any logo or trademark (no Triforce, no game logo).

## How the work is split

- **Claude draws in code (nothing to generate):** switches, slider, badges, filter chips, item slots (empty, owned,
  selection ring), list-row buttons (normal, selected, pressed), the footer and header bars, the parchment panel,
  the dividers, the arrows, lock and tick, and all button glyphs (A, B, X, Y, L, R, ZL, ZR, +, −, R3, D-pad).
- **The AI generates, as 11 sheets (section 3):** the carved frame corners and edges, the tab, row and age icons,
  about 90 item icons, and the logo.
- **A script cuts the sheets** into separate PNGs and removes the magenta background. Do not cut anything by hand.

## 1. Style (put this at the start of every prompt)

> Original fantasy game UI artwork in the style of a polished modern remake of a classic adventure game: warm,
> handcrafted, storybook feel. Dark carved wood, gold-brown trim, aged parchment, cream highlights. Soft light from
> the top left, subtle bevels, thin dark outline, 2 to 3 tones per object, flat clean shapes with gentle shading.
> No text, no letters, no numbers, no labels, no watermark. Original designs only: do not copy any existing game's
> artwork, characters, logos or symbols (no Triforce).

Palette to stay inside: deep wood `#2B1E13`, lighter wood `#4A3522`, trim gold `#D9BD6A`, dark gold `#A8893F`,
parchment `#D8C49A` and `#C4AD7C`, ink `#2A1D12`, cream `#F3EAD2`, green accent `#8FD36B`.

**Attach a screenshot of the game's pause menu** as a style reference to every conversation, and say "match this
reference's palette and carved-wood look; make original artwork".

## 2. Rules for every sheet (also paste these)

> Draw the images as ONE sheet on a perfectly flat pure magenta background (#FF00FF) with no gradient, no shadow
> on the background and no texture. Arrange the images in an exact grid of equal square cells, C columns by R rows
> as stated below, left to right then top to bottom, with a wide empty magenta margin around every image. One
> image per cell, centred, filling about 75 percent of the cell, facing the viewer straight on (no perspective,
> no rotation). Nothing may touch or cross a cell edge. No grid lines, no captions. Use no magenta, pink or
> purple inside the images themselves (magenta is the cut-out colour). Output at the highest resolution you can,
> square, PNG.

If a sheet comes out with the wrong count or order, ask for it again; do not fix it by hand. Use **one
conversation per sheet** but always attach the same reference screenshot, and for sheets 3 to 10 also attach
sheet 2 (or the first good sheet) with "keep exactly this icon style".

## 3. The sheets

Each prompt = Style (section 1) + Rules (section 2) + the sheet text below. Cells are listed in order.

### Sheet 1: frame pieces (2 columns x 2 rows)
Straight-on, flat, not photographed; the pieces will be mirrored and stretched by code.
1. The top-left **corner of an ornate menu frame**: dark carved wood with a gold inner trim line and a curling
   scroll flourish in the corner, the straight edges leaving the corner towards the right and downwards. The area
   inside the frame is flat dark wood `#2B1E13`.
2. A **horizontal edge piece** of the same frame, about 3 times as long as it is tall, that repeats seamlessly
   left to right (left and right ends must match exactly).
3. The **left end cap of a header bar**: a lighter wood bar with a gold line along the bottom that ends in a
   swooping scroll curve (like the swoop on a pause-menu title band).
4. A **horizontal middle piece of the header bar**, repeating seamlessly, same wood and gold line.

### Sheet 2: tab icons and age tags (3 columns x 2 rows)
Gold-brown glyphs with a dark outline, each inside a plain round carved-wood medallion.
1. A heroic figure silhouette or a heart in a ring (Player tab). 2. A leather satchel (Items tab). 3. A small
globe, or sun and moon (World tab). 4. A map pin on a swirl (Teleport tab). 5. A single green leaf emblem
(child tag). 6. A small sword emblem (adult tag).

### Sheet 3: row icons (4 columns x 3 rows)
Same medallion style as sheet 2.
1. A heart with a small shield (God Mode). 2. A single green rupee gem. 3. A green potion bottle (magic).
4. Arrows crossed with a round bomb (items). 5. A feather. 6. An hourglass (switch age). 7. A sun and moon
clock (time). 8. An eye inside a square frame (camera). 9. A small flag on a pole (mark). 10. A curved arrow
pointing to a flag (return). 11. A small scroll with an "i"-shaped quill mark, no letters (info). 12. Leave the
cell empty.

### Sheet 4: equipment (4 columns x 3 rows), full-colour item icons
1. Plain short sword with wooden grip (Kokiri). 2. Elegant blade with a violet hilt (Master). 3. Large curved
knife with a broken-looking tip (Giant's). 4. Huge two-handed greatsword (Biggoron's). 5. Round wooden shield.
6. Kite-shaped blue shield with a red bird crest. 7. Mirror-polished silver shield. 8. Red-brown tunic.
9. Blue tunic. 10. Heavy iron boots. 11. Light boots with small wings. 12. Leave the cell empty.

### Sheet 5: items, part 1 (4 columns x 3 rows)
1. Wooden stick. 2. Brown nut. 3. Round black bomb with a lit fuse. 4. Small mouse-shaped missile (Bombchu).
5. Y-shaped wooden slingshot. 6. Small pile of seeds. 7. Wooden bow. 8. Single arrow. 9. Arrow with fire.
10. Arrow with ice crystals. 11. Arrow with golden light. 12. A swirling orange flame (Din's Fire).

### Sheet 6: items, part 2 (4 columns x 3 rows)
1. A green swirling wind crystal (Farore's Wind). 2. A blue crystal (Nayru's Love). 3. A small turquoise
clay ocarina. 4. A blue ocarina with a gold mouthpiece. 5. A hookshot (chain with a claw). 6. A longer hookshot
with a larger claw. 7. A boomerang. 8. A round lens with an eye pattern. 9. A seed with a sprout (bean).
10. A big mallet. 11. An empty glass bottle with a cork. 12. Leave the cell empty.

### Sheet 7: masks (4 columns x 2 rows)
Eight friendly, distinct fantasy masks: 1. a yellow-cheeked mask with a bushy moustache; 2. a skull mask;
3. a spooky grinning mask; 4. a bunny-ears hood; 5. a stone-faced mask; 6. a blue fish-like mask;
7. a desert veil-style mask; 8. a golden face-mask with a closed eye.

### Sheet 8: upgrades (5 columns x 4 rows)
1. A strong gauntlet bracelet. 2. Silver gauntlets. 3. Golden gauntlets. 4. A silver fish-scale.
5. A golden fish-scale. 6–8. Three bomb bags getting bigger (small, medium, large). 9–11. Three arrow quivers
getting bigger. 12–14. Three seed pouches getting bigger. 15. A modest wallet. 16. A bulging giant wallet.
17. A small green magic meter vial. 18. A larger double magic vial. 19. A shield-shaped crest with two
layers (double defense). 20. Leave the cell empty.

### Sheet 9: stones, medallions and quest (4 columns x 3 rows)
1. A green emerald stone. 2. A red ruby. 3. A blue sapphire. 4–9. Six ring-shaped medallions in green, red,
blue, purple, orange and yellow, each with a different simple engraved symbol. 10. A small stone with an eye
(Stone of Agony). 11. A membership card with a small red emblem. 12. A golden skull-shaped token.

### Sheet 10: songs (4 columns x 3 rows)
Twelve small music motifs (a musical staff with notes, or a single large note), no letters, each in a colour
that matches its meaning: 1 a soft pink lullaby; 2 a brown horse-themed song; 3 a green forest-green song;
4 a golden sun song; 5 a blue time song; 6 a grey storm song; 7 green; 8 red; 9 blue; 10 purple; 11 orange;
12 yellow (the last six matching the six medallions' colours).

### Sheet 11: brand (2 columns x 1 row)
1. The word "Wayfarer" in a hand-lettered fantasy serif, cream with a thin dark outline, with a small compass
rose to its left. (This is the only image allowed to contain letters.) 2. The compass rose alone as a round
emblem.

## 4. Fonts (not art, but needed)

Pick two **free-to-embed** fonts (SIL Open Font Licence or similar) and send the files with their licences: a
decorative serif for titles (for example Cinzel) and a clean readable body font (for example Alegreya Sans or
Lora). They must contain Latin plus Portuguese accents (á é í ó ú â ê ô ã õ ç).

## 5. Delivery

- Send each finished sheet as one PNG, named `sheet01_frame.png` … `sheet11_brand.png`, in a folder
  `wayfarer-art/`.
- Add a short note saying which tool made them, so the licence can be recorded in the repository.
- Nothing needs to be cut, resized or made transparent: the script does that.
- Claude's own pieces (section "How the work is split") are done in code once the sheets are in.
