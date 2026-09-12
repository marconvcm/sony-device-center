# dmgbuild settings for the Sony Device Center disk image.
#
# Driven by build-dmg.sh, which runs from this directory so the background and
# icon resolve as plain filenames:
#
#   dmgbuild -s dmgbuild.py -D app="/path/to/Sony Device Center.app" "Sony Device Center" out.dmg
#
# The window is 660x400 points. gragen.py paints the background on the same
# grid, so the glow sits behind the app and the arrow lands between the icons.

import os.path

app = defines["app"]  # noqa: F821 - injected by dmgbuild

format = "UDZO"
size = None  # let hdiutil pick

files = [app]
symlinks = {"Applications": "/Applications"}

icon = "AppIcon.icns"
background = "dmg-background.png"  # dmgbuild picks up dmg-background@2x.png as well

show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
sidebar_width = 180

window_rect = ((200, 160), (660, 400))
default_view = "icon-view"
icon_size = 128
text_size = 13
arrange_by = None
grid_offset = (0, 0)
grid_spacing = 100
scroll_position = (0, 0)
label_pos = "bottom"

icon_locations = {
    os.path.basename(app): (175, 185),
    "Applications": (485, 185),
}
