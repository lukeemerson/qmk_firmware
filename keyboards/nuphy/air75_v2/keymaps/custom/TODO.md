# TODO

## Open
- [ ] Raycast's own snippets: fold in from a plain JSON export (snippets D6)
- [ ] Fix Better Aliases labels: `gee`/`ge3`/`ge4` ("1/2/3") and `gv3`/`gvv` ("code"/"git"); `oA` vs `oa` differ only by case
- [ ] Symbol layer (lighting: orange + hot pink mix)
- [ ] Check Zellij passes focus events (`:autocmd FocusLost * echo "lost"`)
- [ ] Decide: WPM Heat start at dark orange instead of mint?
- [ ] Fix: Mod Legend uses a 16-bit timer (can trigger early after ~65 s idle)
- [ ] Maybe: Fn+G only switches with no keys held

## Done
- [x] `~/.config/nvim` backed up to the private github.com/lukeemerson/nvim-config
- [x] Snippets source of truth: private `~/.config/snippets` (github.com/lukeemerson/snippets) generates Better Aliases and nvim snippets plus a Wispr checklist; regenerates the leader codes on every `qmk compile`
- [x] Right Cmd leader: tap sends Hyper+Space (Better Aliases Expand Alias), gold legend lights the keys that continue a code, `,` + code pastes
- [x] Main base: dark orange per-key flash (75%, 500 ms fade), no background, rails 50% → 75% while typing
- [x] Game modes on Fn+G (Factorio, Valheim), nav layer off in games
- [x] Overlays: Layer Legend, Mod Legend, Wispr, Battery (Fn+\), WPM Heat (Fn+W)
- [x] Bases: Night (Fn+N), Study with Anki keys (Fn+S)
- [x] Focus timer (Fn+P, 25 min, teal, any base)
- [x] Neovim mode lighting over Raw HID (main + unfilt configs)
