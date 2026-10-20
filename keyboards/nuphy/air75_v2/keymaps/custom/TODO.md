# TODO

- [x] Main base redesigned: amber glow, F/J bright, soft ripples, rails 50% rising to 65% while typing
- [x] Gaming vs work modes with distinct color schemes (Fn+G: Factorio, Valheim, off)
  - Fn+G toggles modes (switch only with no keys held)
  - Work: Caps tap Esc / hold nav layer
  - Gaming: Caps plain Esc, nav layer off
  - Each mode gets its own lighting color (NuPhy RGB/side-light driver in `ansi/`)
  - Gaming and symbol layers: orange + hot pink mix (nav is violet, Fn is red)
- [x] Built from `lighting-mockups.html`: Layer Legend, Mod Legend, Wispr, Battery (Fn+\), WPM Heat (Fn+W), Night (Fn+N), Factorio + Valheim (Fn+G)
- [ ] Focus Timer (not picked yet)
- [ ] Snippets as part of the build: one `snippets.json` source of truth
  - Generates Better Aliases `config.json`, nvim `snippets/markdown.json`, a Raycast snippet import file, and a Wispr checklist (don't write Wispr's `flow.sqlite`, it cloud-syncs)
  - QMK Leader on Right Cmd (tap Leader, hold Cmd): Leader + code fires Better Aliases "Expand Alias" via a Raycast hotkey and types the same code
  - Flagged drift: nvim `drug`/`perm` vs Wispr vault snippets, `bx`/`px` and `cs`/`ss` duplicates, `gee`/`ge3`/`ge4` and `gv3`/`gvv` labels
