# Third-Party Reference Notes

## IgorKhramtsov/DeEsser

VVChain's De-Esser is an independent C++/JavaScript reimplementation of the publicly visible processing structure in the referenced project:

- adjacent-sample difference thresholding / sibilance-area detection
- 8192-sample processing blocks
- FFT
- frequency-dependent suppression around a male/female target frequency
- inverse FFT
- Male Vocal / Female Vocal target selection
- Intensity control
- Average Offset control

The referenced repository branch inspected for this implementation did not contain a license file. VVChain therefore does **not** copy its source files verbatim; it reimplements the observable algorithm and interface behavior.

Reference:
https://github.com/IgorKhramtsov/DeEsser

## xmikos/qspectrumanalyzer

The referenced QSpectrumAnalyzer repository is GPL-3.0. VVChain does **not** include its GPL source code.

VVChain instead implements the requested analyzer behavior natively with JUCE's FFT and an equivalent spectrum/waterfall presentation:

- FFT spectrum
- dB scale
- frequency scale
- waterfall history
- averaging
- peak hold
- persistence
- smoothing

Reference:
https://github.com/xmikos/qspectrumanalyzer

License of the reference project:
GPL-3.0
