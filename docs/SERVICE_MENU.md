# Service Menu

Current behavior of the Service Menu. The C++ class is `ServiceMenu`;
the player-facing interface is the Service Menu.

## Layout and text

The left panel holds the artisan name and rank, the Craft/Improve tabs
(Smithing), the category tabs, the search field with the Material or Effect
filter, the column titles and the list. The right side holds the 3D preview and
the item card. Panels use a double border with Journal-style corners; dividers
are plain double lines. On wide screens the list follows the left edge and the
details the right edge, up to a fixed distance.

Titles are in capitals: column titles, category and service tabs, card labels,
section titles and confirmation headings. Values, buttons, prompts and messages
use sentence case; a value that arrives entirely in capitals is shown in title
case. Item, NPC and place names keep their source spelling.

Smithing shows Damage or Armor, Weight, Price, and Delivery. Improve shows
current/projected Damage or Armor, Result Quality, and Price. Enchanting item
selection shows item statistics; effect selection shows the effect description
and individual Price. Alchemy shows the name with `(x5)`, Effect, and batch
Price. Orders show Status, Time left, and Pickup. Notices and the reason a row
is unavailable appear in two lines below the list.

## Controls

| Action | Keyboard | Xbox | PlayStation |
|---|---|---|---|
| Browse | Arrows / wheel | D-pad / left stick | D-pad / left stick |
| Category | Left / Right | D-pad Left / Right | D-pad Left / Right |
| Select / confirm | Enter | A | Cross |
| Back / cancel | Esc | B | Circle |
| Filters list | F | Y | Triangle |
| Craft / Improve tab | Tab | LB / RB | L1 / R1 |
| Enchanting Craft | C | X | Square |
| Rename in Enchanting confirmation | R | X | Square |
| Rotate preview | hold mouse button and drag | right stick | right stick |

The filters list holds the sort order and the Material or Effect filter. Up/Down
chooses, confirm applies, Back closes. Clicking a column title sorts by that
column; clicking it again reverses the order. An arrow marks the sorted column.

The search field (mouse) filters the list by name while typing. Enter keeps the
search; Esc clears it. Gameplay text input is enabled only while typing and is
released on Enter, Esc, page change and close.

Mouse: hovering a row selects it; clicking the selected row uses it. The wheel
moves the selection from anywhere on the menu, and hover selection pauses until
the mouse moves, so scrolling is never undone by the row under the pointer.
Click callbacks are deferred to the next frame so an event target is not
destroyed inside its own callback. Confirmation overlays intercept background
clicks.

Intentional gamepad input hides the cursor. Mouse clicks/wheel or accumulated
movement of at least six units within 150 ms restores it. The left stick uses a
0.65 deadzone, a 700 ms initial repeat delay and 350 ms repeat intervals.

Rename edits the confirmation's item-name field. Enter/A/Cross accepts the
edit and Esc/B/Circle cancels it. A separate Craft confirmation performs the
transaction. The accepted name is stored through vanilla `ExtraTextDisplayData`.

## Key icons

The movie draws its own icons in the game's style: black key caps with white
text (keyboard), black face buttons with white letters (Xbox), black buttons
with the Cross/Circle/Square/Triangle symbols (PlayStation), and shoulder buttons
as key caps. No controller artwork is imported from other menus or SkyUI.
PlayStation labels follow the platform supplied to Scaleform; a controller
exposed as XInput reports Xbox.

## 3D preview

The item model uses the game's inventory 3D manager. Its anchor is placed once
on the centre of the preview area and restored on close. Rotation is handed to
the game's own inventory handler; when the game refuses the input, a fallback
rotates around the model's bound centre.

## Translation

The movie writes its fixed texts in English through `T("...")`. The plugin sends
the translated texts with each page (`labels`), and translates row values,
notices and messages before publishing them. See
`installer/README - Translations.md`.

## Data and feedback

Controllers retain pricing and gameplay rules. `ServiceRow`/`ServicePage` carry
structured values; ActionScript never parses a detail paragraph for gameplay.
Commands only identify an existing row; prices and items are decided natively.
