# kodoku
##### Keeping your home directory clean since 2022

This is a LD\_PRELOAD module intended to minimize clutter in $HOME, by giving
every program a separate $HOME to clutter.

When a process is started (right before exec), its home directory is set to
`$KODOKU_HOME/$(basename $(realpath $0))`. The directory is not automatically
created; otherwise you'd end up with a lot of empty directories like `cat` and
`ls`, which don't use any external files.

It's possible that certain programs get upset if `$HOME` does not exist; in
that case you can create the directory manually, and perhaps submit a pull
request to the offending program.

There are places where LD\_PRELOAD does not apply, including but not limited to
setuid binaries, statically linked stuff (which mostly applies to Golang
programs), and programs executed with kodoku cleared from LD\_PRELOAD. These
are delegated to `$KODOKU_HOME/$KODOKU_MISC`, which defaults to
`$KODOKU_HOME/misc`.

## Installation

Build the .so via `make`, then insert it into your environment via your preferred method.
You will also need to set the `KODOKU_HOME` variable, likely via the same environment mechanism.

Personally I use `~/.pam_environment`:

```pam_env
LD_PRELOAD    DEFAULT=@{HOME}/lib/kodoku/kodoku.so
KODOKU_HOME   DEFAULT=@{HOME}/data
```

It might also be handy to set `USER_HOME DEFAULT=@{HOME}`. Kodoku doesn't use
it, but it can be useful for use in other programs.

## Compatibility

This setup actually causes fewer issues than I would have expected.
Nevertheless, there are still a number of situations that do not work quite
satisfactorily.

Most of these issues arise from programs expecting to share the same directory.
This can be simply be multiple programs that expect to share their data (such
as `python` and `pip`), or libraries that have their own configuration. These
issues can usually be solved either via gratuitous use of symlinks, or if
supported, via environment variables.

Another class of issues include programs that expect `~` to point to a
directory useful for users. This includes graphical file choosers such as those
included in GTK and Qt, as well as many CLI tools such as `zsh` and `vim`.

I have some integrations and workarounds in my dotfiles. They're not really
documented though.
