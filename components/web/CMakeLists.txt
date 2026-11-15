set(web_assets "assets/index.html" "assets/app.css" "assets/app.js")
if(EXISTS "${COMPONENT_DIR}/assets/server.crt" AND EXISTS "${COMPONENT_DIR}/assets/server.key")
    list(APPEND web_assets "assets/server.crt" "assets/server.key")
endif()

idf_component_register(SRCS "web_server.c" "web_auth.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_https_server esp_timer esp_hw_support json settings connectivity sensor weather app_controller
                       EMBED_TXTFILES ${web_assets})
