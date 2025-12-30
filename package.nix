{
  lib,
  stdenv,
  libX11,
  libXScrnSaver,
}:

stdenv.mkDerivation {
  pname = "xsscmd";
  version = "0.1";

  src = lib.sourceByRegex ./. [
    "Makefile"
    "xsscmd\\.c"
  ];

  buildInputs = [
    libX11
    libXScrnSaver
  ];

  makeFlags = [
    "CFLAGS=$(NIX_CFLAGS_COMPILE)"
    "PREFIX=$(out)"
  ];

  meta = {
    description = "Run shell commands based on X11 Screen Saver extension's idle time.";
    license = lib.licenses.beerware;
    maintainers = with lib.maintainers; [ schnusch ];
    platforms = lib.platforms.linux;
    mainProgram = "xsscmd";
  };
}
