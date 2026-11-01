# chatterino-slim user guide

chatterino-slim is Chatterino 2 with options to hide everything around the chat, and with shared windows that later launches add tabs to. Everything else works as in Chatterino. For the rest of the app, see the [Chatterino wiki](https://wiki.chatterino.com).

## Settings and flags

Each slim option is both a setting and a command-line flag. The option is on if either one turns it on. Flags apply to the current session only and never change saved settings, so a launcher can start a slim window without changing how Chatterino looks when you open it yourself.

| Option | Setting (Settings → General) | Flag | `settings.json` key |
| --- | --- | --- | --- |
| Hide the channel header above each chat | Advanced → Chat title → **Show chat title** (off) | `--hide-split-header` | `appearance.splitheader.hide` |
| Hide the settings button | **Hide preferences button** | `--hide-header-buttons` | `appearance.hidePreferencesButton` |
| Hide the account button | **Hide user button** | `--hide-header-buttons` | `appearance.hideUserButton` |
| Hide the `+` button after the tabs | Tabs → **Show add tab button** (off) | `--hide-add-tab-button` | `appearance.hideAddTabButton` |
| Show the tab row only from the second tab on | Tabs → **Hide tabs while only one is open** | `--hide-single-tab` | `appearance.hideSingleTab` |
| Tabs share the full window width | Tabs → **Stretch tabs to the window width** | `--fill-tabs` | `appearance.fillTabWidth` |
| All of the above | | `--slim` | |
| Save settings in windows opened with `-c` | Advanced → Windows opened with -c → **Save settings changed in these windows** | `--save-settings` | `misc.saveSettingsWithChannels` |

The settings and account buttons come from upstream Chatterino. The flag hides both at once. Even with the flag set, the account button comes back when your login expires, so you can sign in again.

`--slim` doesn't include `--save-settings`. See [Saving settings](#saving-settings).

What stays visible in the slimmest layout: the chat, the input box and the emote button. The hidden parts stay reachable through hotkeys: Ctrl+P opens the settings, Ctrl+Shift+T opens a new tab, Ctrl+R changes the channel of the focused chat and Ctrl+W closes it. For the rest of the channel menu, turn the header back on.

When tabs are stretched, every tab gets the same width as long as all titles fit. If they don't, tabs keep their natural width, wrap onto more rows, and each row is stretched to the window width.

### Hiding all tabs

Ctrl+U hides the whole tab row, or shows it again. This fork remembers the choice across restarts (`appearance.hideAllTabs`). While all tabs are hidden, **Hide tabs while only one is open** has no visible effect, so press Ctrl+U to bring the row back if tabs don't appear.

## Shared instances

Give a window a name with `--instance`, and later launches with the same name hand their channels to it instead of opening another window:

```sh
# Starts a window named "streams" with one tab
chatterino --slim --instance streams -c t:lirik

# Each of these exits right away; the "streams" window does the work
chatterino --instance streams -c t:summit1g          # opens a tab, or selects it if it's open
chatterino --instance streams -c 't:eliasn97;t:stegi' # several at once
chatterino --instance streams --remove t:summit1g    # closes that tab
```

How it behaves:

- **Adding.** For each channel in `-c`, the window selects a tab that shows only that channel, or adds a new tab for it.
- **Removing.** `--remove` closes every tab where all chats show that channel. Tabs with other chats in them stay open. Channel names are matched without regard to case.
- **Last tab.** When `--remove` closes the last tab, the window closes and Chatterino quits.
- **Nothing running.** `--remove` without a running instance does nothing and exits. `-c` without a running instance starts a new instance with that name.
- **Names.** 1 to 64 letters, digits, `_` or `-`. An invalid name is ignored with a warning, and Chatterino starts normally.
- **Privacy.** The instance listens on a socket in `$XDG_RUNTIME_DIR` (usually `/run/user/<uid>`) that only your user can open.

Only Twitch channels (`t:`) are supported, as with `-c` in upstream Chatterino.

`-c` starts a session that doesn't save its layout or settings, as in upstream Chatterino. A shared instance started with `-c` therefore forgets its tabs when it quits. To keep settings you change in it, see [Saving settings](#saving-settings).

## Saving settings

With `--save-settings`, or with the setting on, a window opened with `-c` saves the settings you change in it, and its settings dialog opens. It still doesn't save its tabs, so the layout of your normal Chatterino window stays as it is.

Several Chatterino windows often run at once and share one `settings.json`. A window with this option writes only the settings it changed: it locks the file, reads it again, applies its own changes and leaves everything else as the file has it. So closing a window doesn't undo what you changed in another one, and it doesn't bring back a login you replaced elsewhere. Changes made in one window show up in the others after they restart.

Windows without the option write all their settings, as upstream Chatterino does. If you also keep a normal Chatterino window open, turn the setting on there too (it takes effect after a restart), or that window can undo changes from the others when it saves.

## Launcher setups

A launcher that opens streams can use either style:

- **One chat window for all streams:** use one fixed name.

  ```sh
  chatterino --slim --instance streams -c "t:$channel"        # stream opened
  chatterino --instance streams --remove "t:$channel"         # stream closed
  ```

- **One chat window per stream:** use a name per channel, so each window can be closed on its own.

  ```sh
  chatterino --slim --instance "stream-$channel" -c "t:$channel"
  chatterino --instance "stream-$channel" --remove "t:$channel"
  ```

To place the window with your window manager, match its window class: `com.chatterino.chatterino` on Wayland. All Chatterino windows share that class, including the settings dialog and popups.

## Troubleshooting

**Chatterino doesn't start, and a second launch opens another window.** On Linux, Chatterino checks for WebP image support at startup and shows a dialog if it's missing. The dialog blocks startup before the instance is ready. Install your distribution's Qt image formats package, for example `qt6-imageformats` on Arch Linux.

**A launch with `--instance` opened a separate window.** The first window was started without `--instance`, or with another name, or the name was invalid. Run the launch in a terminal and check for a warning.

**Settings changes are gone after a restart.** That session was started with `-c` and without `--save-settings`. See [Saving settings](#saving-settings).

**No tabs appear even with several chats open.** All tabs are hidden with Ctrl+U. Press Ctrl+U again.

**Upstream Chatterino and chatterino-slim share settings.** Both use the same settings folder unless one of them is the Flatpak, which keeps its own. Settings for the slim options are ignored by upstream Chatterino. Don't run both against the same folder at the same time: upstream Chatterino writes all its settings and can undo changes saved by chatterino-slim windows.
