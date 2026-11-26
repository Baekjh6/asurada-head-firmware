#!/usr/bin/env python3
"""Turn a microWakeWord streaming model (.tflite) into src/wake/wake_model.h for the asurada_v03 sketch.

    python extras/make_wake_model_header.py my_model.tflite --name "My Phrase" --cutoff 0.97

Run it from the sketch folder (the one with asurada_v03.ino). It overwrites src/wake/wake_model.h.
--name   is only printed on the serial port when the wake word is heard.
--cutoff is the detection threshold, 0 to 1 (the average of the last 5 inferences must exceed it).
"""
import argparse
import os

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument('model', help='microWakeWord streaming model (.tflite)')
ap.add_argument('--name', default='Wake word', help='text shown in the serial log')
ap.add_argument('--cutoff', type=float, default=0.97, help='detection threshold, 0 to 1')
ap.add_argument('--out', default=os.path.join('src', 'wake', 'wake_model.h'))
a = ap.parse_args()

data = open(a.model, 'rb').read()
if not 0.0 < a.cutoff <= 1.0:
    raise SystemExit('--cutoff must be between 0 and 1')
if '"' in a.name or '\\' in a.name:
    raise SystemExit('--name must not contain quotes or backslashes')

lines = ['// Made by extras/make_wake_model_header.py. Do not edit by hand.',
         '// model: %s (%d bytes)' % (os.path.basename(a.model), len(data)),
         '#pragma once',
         '#define WAKE_MODEL_NAME "%s"' % a.name,
         '#define WAKE_MODEL_CUTOFF %.2ff' % a.cutoff,
         'static const unsigned int wake_model_len = %d;' % len(data),
         'alignas(16) static const unsigned char wake_model_data[] ' + '= {']
for i in range(0, len(data), 20):
    lines.append('  ' + ','.join('0x%02x' % b for b in data[i:i + 20]) + ',')
lines.append('};')
with open(a.out, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(lines) + '\n')
print('wrote %s (%d bytes of model, cutoff %.2f)' % (a.out, len(data), a.cutoff))
