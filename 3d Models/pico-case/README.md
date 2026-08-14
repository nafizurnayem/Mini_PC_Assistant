# Pico Assistant — tilting L-base case

A parametric 3D-printable enclosure for the Pico Assistant hardware: a 3.2" ILI9341
SPI TFT in portrait, an ESP32-S3 Super Mini, a TTP223 touch pad that senses through
the front wall, an EC11 encoder for volume, and a buzzer behind a side grille.

The screen module pivots on an L-shaped base and locks anywhere between **90° (upright)
and 45°**, with soft detents every 15°.

|  |  |
|---|---|
| Enclosure | 60.6 × 131.3 × 22.0 mm |
| Base | 82 × 82 × 58 mm |
| Viewing window | 49.8 × 66.0 mm (active area + 0.6 mm all round) |
| Parts to print | 5 (front shell, back shell, base, knob, 2× thumbwheel) |
| Fasteners | 8 × M3 heat-set inserts, 8 × M3 screws |
| Supports | none |

## Files

```
pico-case/
├─ PicoAssistantCase/
│  ├─ PicoAssistantCase.py    ← run this in Fusion 360
│  └─ PicoAssistantCase.manifest
├─ verify_layout.py           clearance + collision checker (plain Python)
├─ verify_render.py           slices the same geometry to PNG (plain Python)
├─ preview/*.png              generated sections, see below
└─ README.md
```

## Running it in Fusion 360

1. **File → New Design.**
2. **Utilities → ADD-INS → Scripts and Add-Ins.**
3. On the *Scripts* tab press the green **+** next to "My Scripts", and pick the
   `PicoAssistantCase` **folder**.
4. Select `PicoAssistantCase` in the list and press **Run**.

It builds six components in about ten seconds and reports the key dimensions.
`90_MockDisplay` and `91_MockElectronics` are dummy solids of the TFT, the ESP32,
the encoder and the buzzer — spin the model around to check the fit visually, then
hide or delete them before exporting.

To change anything, edit the constants at the top of the script, delete the bodies,
and run it again. Every dimension in the model is derived from that block; nothing
is hard-coded further down.

### Parameters you are most likely to want to touch

| Parameter | Default | Why |
|---|---|---|
| `TTP_W`, `TTP_H` | 15.3 × 11.3 | **Measure your TTP223 board.** Breakouts vary a lot; the ribs must match. |
| `TOUCH_MEM_T` | 1.2 | Wall the touch pad senses through. Thicker = less sensitive. |
| `FIT` | 0.40 | Per-side clearance around the display PCB. Raise to 0.5 if your printer runs tight. |
| `EC11_BODY_H` | 6.8 | Encoder body height below the panel. |
| `EC11_PIN_HOLE` | `False` | Set `True` if you want a hole for the anti-rotation pin instead of snipping it off. |
| `BUZ_D` | 12.4 | Buzzer diameter. |
| `TILT_MIN` / `DETENT_STEP` | 45 / 15 | Tilt range and detent spacing. |
| `CHIN` | 26.0 | Bezel below the screen. Drives overall height. |

## Design notes

**Display seating.** The LCD body (55.04 × 77.7 mm) drops into a pocket 2.65 mm deep,
so the glass sits 0.35 mm behind the front wall. The PCB is 5.8 mm taller than the LCD
body at each end, and those two strips land on a ledge at z = −5.15 — that is what sets
the screen depth, not the PCB edges. Four Ø6 posts on the back shell stop 0.1 mm short
of the PCB and hold it against that ledge; a strip of foam tape on the post tops takes
up print tolerance.

**Touch.** The chin is thinned to a 1.2 mm membrane over a Ø22 circle with a shallow
Ø24 dish on the outside, so your finger finds the target without a hole in the case.
Four ribs locate the TTP223 pad-side down against the membrane.

**Headers.** The STEP model shows the pins protruding 8.6 mm behind the PCB; the rear
cavity is 12.75 mm, leaving 4.15 mm for wire bends.

**Hinge.** Two Ø11 bearing pads stand 4 mm off each side wall with M3 inserts in blind
holes — 5.4 mm deep into 6.4 mm of material, so nothing breaks into the cavity. The
base cheeks bear only on those two pads, which is what keeps the joint from rocking.
The lock screw runs in an arc 17 mm from the pivot; the arc is Ø3.05 with Ø3.5
scallops at 90/75/60/45°, so an M3 shank passes with light finger pressure but will not
creep under the weight of the screen.

**Cable.** USB-C exits the bottom wall and the base has a 14 × 12 mm slot behind the
cheeks, so the lead drops straight through the desk-side of the plate.

## Bill of materials

| Qty | Item | Notes |
|---:|---|---|
| 8 | M3 brass heat-set insert, 5.0 mm long, Ø4.6 OD | holes are Ø4.0 |
| 4 | M3 × 16 countersunk machine screw | back shell → front shell |
| 2 | M3 × 12 socket cap + M3 washer | hinge pivot |
| 2 | M3 × 20 **hex head** bolt | tilt lock, captured in the printed thumbwheels |
| 4 | Ø10 × ~1 mm silicone foot | recesses provided |
| — | double-sided foam tape | TTP223, PCB press posts |

## Print settings

| | |
|---|---|
| Material | PETG or PLA+ |
| Nozzle / layer | 0.4 mm / 0.20 mm (0.16 mm for the front shell) |
| Walls | 3 perimeters, 5 top and bottom layers |
| Infill | 25% gyroid; 40% for the base |
| Supports | **none required** |

Orientation:

* **Front shell** — front face on the bed. The bezel step, touch dish and window all
  print against the plate; bosses and ribs point up.
* **Back shell** — outer back face on the bed. The countersinks open downward, the
  rail grooves are 1.2 mm bridges.
* **Base** — plate underside on the bed, cheeks vertical.
* **Knob** — bore opening on the bed. **Thumbwheels** — flange on the bed.

## Assembly

1. Melt the 8 inserts in: four in the front-shell bosses, four in the hinge pads on
   the back shell. Keep them square and flush.
2. Solder the loom to the display before it goes in — the header is on the bottom
   edge and gets tight once the shells close.

   | ILI9341 | ESP32-S3 Super Mini |
   |---|---|
   | VCC | 5V |
   | GND | GND |
   | CS | GPIO 10 |
   | RESET | GPIO 6 |
   | DC / RS | GPIO 7 |
   | MOSI / SDI | GPIO 11 |
   | SCK / CLK | GPIO 12 |
   | LED | 3V3 (OUT) |

   TTP223 signal → GPIO 4, powered from **3V3**. Buzzer → GPIO 2. GPIO 13 stays
   unconnected (dummy MISO).
3. Lay the display glass-down into the front-shell pocket.
4. Press the TTP223 into its ribs, copper pad facing the membrane, foam tape behind it.
5. Slide the ESP32-S3 down the back-shell rails, USB-C toward the bottom opening.
6. Fit the EC11 through the top wall, tighten its nut, push the knob on.
7. Drop the buzzer into the cradle in the brow; a dab of hot glue holds it.
8. Add foam tape to the four press posts, close the shells, and drive the four M3 × 16
   countersunk screws from the back.
9. Bolt the enclosure between the base cheeks: M3 × 12 + washer through the pivot each
   side, M3 × 20 through the arc slot into a thumbwheel each side.
10. Stick the feet on. Loosen both thumbwheels a half turn to change the angle.

## Verification

Two plain-Python scripts run outside Fusion and read their parameters straight out of
`PicoAssistantCase.py`, so they cannot drift from the model.

```bash
python verify_layout.py     # 27 volumes checked pairwise + wall thicknesses + tilt sweep
python verify_render.py     # writes preview/*.png sections of the real CSG result
```

`verify_render.py` re-executes the *same* builder functions against a small
point-membership CSG kernel, so the PNGs show what Fusion will actually produce.

| File | Shows |
|---|---|
| `01_front_shell_slices.png` | z = −0.5 / −3.5 / −6.0 / −7.5 — window, LCD pocket, PCB ledge, lip rebate |
| `02_back_shell_slices.png` | z = −7.0 / −11 / −16.5 / −19 — lip, columns, encoder, rails, buzzer, grille |
| `03_enclosure_sections.png` | vertical sections at x = 0 / 11 / 24 / 29 |
| `04_base.png` | plate plan, cheek elevation with the detent arc |
| `05_hardware.png` | knob and thumbwheel |
| `06_tilt_positions.png` | assembled elevation at 90 / 75 / 60 / 45° |

Current status: **PASS** — no collisions, minimum wall 1.0 mm (hinge insert to cavity),
enclosure clears the plate by 3.4 mm at the worst angle (75°) and the centre of mass
stays 36 mm inside the rear edge at 45°.

## Source geometry

Display dimensions were extracted from `../3-2-tft-spi-240x320-v1-0-1.snapshot.1/320x240.step`:

```
PCB            89.30 × 55.04 × 1.60 mm
LCD body       77.70 × 55.04, 2.30 mm above the PCB, centred
Active area    64.80 × 48.60, centred laterally, 2.95 mm off-centre lengthwise
Headers        14-pin and 4-pin on opposite short edges, 8.54 mm pin protrusion
```

No mounting holes exist in that model, so the case retains the PCB by its edges and
faces rather than by screws through the board.
