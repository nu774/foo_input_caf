foo_input_caf
=============

What is this?
-------------
foobar2000 input plugin that support decoding of CAF (Core Audio Format) files.
Previous versions upto 0.0.10 were dependent on Apple Application Support files, but now it has become completely independent from them.

Supported codecs
----------------
- LPCM
- MPEG audio (MP1, MP2, MP3)
- AAC (LC, SBR, SBR+PS)
- ALAC
- IMA4:1

MPEG audio, AAC, and ALAC are decoded through foobar2000's builtin
packet decoders. Decoder for IMA4:1 is implemented in foo_input_caf.
