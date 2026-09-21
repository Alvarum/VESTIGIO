cmake_minimum_required(VERSION 3.28)
project(VestigioExternalConsumer LANGUAGES C)

find_package(Vestigio 0.1 CONFIG REQUIRED)

add_executable(vestigio_external_consumer main.c)
target_link_libraries(vestigio_external_consumer PRIVATE Vestigio::Runtime)
set_target_properties(vestigio_external_consumer PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)
