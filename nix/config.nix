
{
  allowUnfree = true;
  allowUnsupportedSystem = true;
  allowInsecure = true;
  allowBroken = true;

  packageOverrides = pkgs: {

      # Not all functionality of SDL2 is needed, so disable some.
      SDL2 = pkgs.SDL2.override {
          alsaSupport       = false;
          dbusSupport       = false;
          udevSupport       = false;
          pulseaudioSupport = false;
          waylandSupport    = false;
          openglSupport     = false;
          #x11Support      = true;
          withStatic        = true;
      };

      # PostgreSQL provides libpq, but we don't want one that requires systemd.
      postgresql = pkgs.postgresql.override {
          enableSystemd = false;
      };

      # Disable the following since the wt package doesn't seem to need them.
      firebird       = null;
      qt48Full       = null;
      libmysqlclient = null;
      pango          = null;
      graphicsmagick = null;
      harfbuzz       = null;

      fcgi           = null;

      # Disable the following which cause static linking to fail.
      audit          = null;

      # Disable some SFML functionality which we don't need.
      #openal         = null;
      #libvorbis      = null;
      #flac           = null;
      #wayland        = null;

      #cmake = pkgs.cmake.override {
      #    useSharedLibraries = false;
      #    isBootstrap = true;
      #    #useOpenSSL = false;
      #    #useNcurses = false;
      #    #useQt4 = false;
      #    #withQt5 = false;
      #};

  };

}

