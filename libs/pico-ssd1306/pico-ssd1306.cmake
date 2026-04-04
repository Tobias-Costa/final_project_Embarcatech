# Define a biblioteca como uma "INTERFACE" ou "STATIC" library
add_library(pico-ssd1306 STATIC
    ${PICO_SSD1306_DIR}/ssd1306.c
)

# Diz ao CMake onde estão os arquivos .h (headers)
target_include_directories(pico-ssd1306 PUBLIC
    ${PICO_SSD1306_DIR}
)

# Vincula as dependências de hardware do SDK do Pico
target_link_libraries(pico-ssd1306 
    pico_stdlib 
    hardware_i2c
)
