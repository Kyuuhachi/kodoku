# kodoku
##### Keeping your home directory clean since 2022

This is a LD\_PRELOAD module intended to minimize clutter in $HOME,
by giving every program a separate $HOME to clutter.

When a process is started (right before exec),
its home directory is set to `$KODOKU_HOME/$(basename $(realpath $0))`.
The directory is not automatically created;
otherwise you'd end up with a lot of empty directories like `cat` and `ls`,
which don't use any external files.

It's possible that certain programs get upset if `$HOME` does not exist;
in that case you can create the directory manually,
and perhaps submit a pull request to the offending program.

There are places where LD\_PRELOAD does not apply,
including but not limited to setuid binaries,
statically linked stuff (which mostly applies to Golang programs),
and programs executed with kodoku cleared from LD\_PRELOAD.
These are delegated to `$KODOKU_HOME/$KODOKU_MISC`, which defaults to `$KODOKU_HOME/misc`.

## Installation

Build the .so via `make`, then insert it into your environment via your preferred method.
You will also need to set the `KODOKU_HOME` variable, likely via the same environment mechanism.

Personally I use `~/.pam_environment`:

```pam_env
LD_PRELOAD    DEFAULT=@{HOME}/lib/kodoku/kodoku.so
KODOKU_HOME   DEFAULT=@{HOME}/data
```

## Compatibility

This setup actually causes fewer issues than I would have expected.
Nevertheless, there are still a number of situations that do not work quite satisfactorily.

Most of these issues arise from programs expecting to share the same directory.
This can be simply be multiple programs that expect to share their data (such as `python` and `pip`),
or libraries that have their own configuration.
These issues can usually be solved either via gratuitous use of symlinks,
or if supported, via environment variables.

Another class of issues include programs that expect `~` to point to a directory useful for users.
This includes graphical file choosers such as those included in GTK and Qt,
as well as many CLI tools such as `zsh` and `vim`.
I will add workarounds for some of those as I solve them.

### dconf
dconf, a configuration system mainly used by GNOME and MATE applications,
uses a binary database file at `~/.config/dconf/user`, which is mmapped for fast reads.
Writes, however, are delegated to a daemon, likely to avoid synchronization problems.
This of course does not work if the daemon is set to use a different home directory,
since that file isn't seen by the clients.

This can be worked around by using a keyfile.
This essentially stores the binary database at `/run/user/n/dconf-service/keyfile/user`
(which is on a temporary filesystem),
but spawns a daemon that synchronizes that database with a persistent textual file at `~/.config/dconf/user.txt`.
I believe this feature is intended for obscure filesystems that do not support mmapping,
but it's also useful for obscure user setups that redefine the concept of a home directory.
It also has the benefit of storing the settings in a textual format, which is easier to handle with version control.

To enable this, you need to set the `DCONF_PROFILE` variable to point to a file containing the database description `service-db:keyfile/user`.
I don't know why you can't just put the description in the envvar directly, but that's to be expected from those GNOME folks.
I recommend putting the file under `dconf-service`, since that's where the database will be stored anyway.
With PAM, this can be specified as `DCONF_PROFILE DEFAULT=${KODOKU_HOME}/dconf-service/profile`.

### Zsh
Likely you do not want `~` to expand to `$KODOKU_HOME/zsh`.
This can be adjusted by simply assigning to `HOME`.
I don't know if it has any effect, but assigning `ZDOTDIR=$KODOKU_HOME/zsh` probably won't hurt.
However be careful that any plugins you have likely use `~` instead of `$ZDOTDIR`, so those are likely going to clutter.
Perhaps a pull request is in order.

Also attached is a `kodoku.zsh`. Sourcing it installs kodoku support into the *dynamic named directories* system, which allows `~[x]` to expand to `$KODOKU_HOME/x`.
