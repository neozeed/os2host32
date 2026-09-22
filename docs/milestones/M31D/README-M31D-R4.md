# Milestone 31D R4 - BIO default GPI foreground fix

R3 fixed OS/2 logical-color translation in WinFillRect and dialog placement.
BIO then exposed a separate default-presentation-space assumption: freshly-created
OS/2 presentation spaces begin with CLR_BLACK as the foreground color. The host
had initialized CompatPS.color to 0 (CLR_BACKGROUND), making BIO's Legend text
blend into its CLR_PALEGRAY background and making chart headers/separators use
the wrong color until the guest explicitly called GpiSetColor.

R4 changes the default CompatPS foreground to CLR_BLACK (-1) in both PMWIN-created
and PMGPI-created presentation spaces. No BIO-specific drawing is hard-coded.

Regression:
    make m31d-gpi-default-check
