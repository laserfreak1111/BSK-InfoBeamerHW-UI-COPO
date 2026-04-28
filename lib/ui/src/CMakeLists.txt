SET(SOURCES ui_splash.c
    ui_scrload.c
    ui_home.c
    ui_mainmenu.c
    ui_dmxmenu.c
    ui_MSG.c
    ui_info.c
    ui_co2menu.c
    ui_valueset.c
    ui_theme_manager.c
    ui_themes.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_bskklein_png.c
    ui_img_fog_png.c
    ui_img_dmxicon_en_png.c
    ui_img_1748321345.c
    ui_img_95950_png.c
    ui_img_dmxicon_di_png.c
    ui_img_1667256483.c)

add_library(ui ${SOURCES})
