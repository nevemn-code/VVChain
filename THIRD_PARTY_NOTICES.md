# Third-Party / Historical Reference Notes

## Current source tree

The current VVChain DSP is independently implemented in this repository. No source files from the historical projects below are incorporated into the active Native or Web DSP.

## Historical De-Esser reference

An earlier design investigation looked at IgorKhramtsov/DeEsser. The current VVChain De-Esser no longer uses that project's FFT/block structure; it is a sample-domain split-band envelope design implemented independently in VVChain.

Reference:
https://github.com/IgorKhramtsov/DeEsser

## Historical spectrum-analyzer reference

An earlier design investigation looked at xmikos/qspectrumanalyzer. The current VVChain editor intentionally has no realtime FFT spectrum/waterfall analyzer and does not include that project's GPL source code.

Reference:
https://github.com/xmikos/qspectrumanalyzer
