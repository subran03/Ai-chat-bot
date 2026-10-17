
import struct
import math
from array import array

from voice.speech_to_text import transcribe_pcm

# Temporary test tone: checks the audio pipeline,
# but it is NOT speech and should not produce words.
SAMPLE_RATE = 16000
DURATION = 2
FREQUENCY = 440
AMPLITUDE = 5000

samples = array("h")

for i in range(SAMPLE_RATE * DURATION):
    value = int(
        AMPLITUDE *
        math.sin(2 * math.pi * FREQUENCY * i / SAMPLE_RATE)
    )
    samples.append(value)

# Ensure the test bytes use little-endian PCM.
if struct.pack("=h", 1) != struct.pack("<h", 1):
    samples.byteswap()

pcm_data = samples.tobytes()

print("Testing the speech-to-text pipeline...")
result = transcribe_pcm(pcm_data)
print("Result:", repr(result))
print("Test finished.")
