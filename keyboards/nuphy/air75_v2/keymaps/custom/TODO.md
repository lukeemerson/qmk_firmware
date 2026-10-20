# TODO

## Open
- [ ] Snippets as part of the build: one `snippets.json` source of truth
  - Generates Better Aliases `config.json`, nvim `snippets/markdown.json`, a Raycast snippet import file, and a Wispr checklist (don't write Wispr's `flow.sqlite`, it cloud-syncs)
  - QMK Leader on Right Cmd (tap Leader, hold Cmd): Leader + code fires Better Aliases "Expand Alias" via a Raycast hotkey and types the same code
  - Flagged drift: nvim `drug`/`perm` vs Wispr vault snippets, `bx`/`px` and `cs`/`ss` duplicates, `gee`/`ge3`/`ge4` and `gv3`/`gvv` labels
- [ ] Symbol layer (lighting: orange + hot pink mix)
- [ ] Back up `~/.config/nvim` (keyboard_lights.lua / .py only exist locally)
- [ ] Check Zellij passes focus events (`:autocmd FocusLost * echo "lost"`)
- [ ] Decide: WPM Heat start at dark orange instead of mint?
- [ ] Fix: Mod Legend uses a 16-bit timer (can trigger early after ~65 s idle)
- [ ] Maybe: Fn+G only switches with no keys held

## Done
- [x] Main base: dark orange per-key flash (75%, 500 ms fade), no background, rails 50% → 75% while typing
- [x] Game modes on Fn+G (Factorio, Valheim), nav layer off in games
- [x] Overlays: Layer Legend, Mod Legend, Wispr, Battery (Fn+\), WPM Heat (Fn+W)
- [x] Bases: Night (Fn+N), Study with Anki keys (Fn+S)
- [x] Focus timer (Fn+P, 25 min, teal, any base)
- [x] Neovim mode lighting over Raw HID (main + unfilt configs)
