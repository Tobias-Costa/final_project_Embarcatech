# Define a biblioteca como uma "INTERFACE" ou "STATIC" library
add_library(qrcode-master STATIC
    ${PICO_SSD1306_DIR}/qrcode.c
)

# Diz ao CMake onde estão os arquivos .h (headers)
target_include_directories(qrcode-master PUBLIC
    ${QR_CODE_MASTER_DIR}
)
