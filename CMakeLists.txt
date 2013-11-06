cmake_minimum_required(VERSION 2.6)
project(qmid)

set(CMAKE_C_FLAGS "-O1 -Wall")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

add_executable(qmid
    qmi_ctl.c
    qmi_dialer.c
    qmi_helpers.c
    qmi_nas.c
    qmi_wds.c
    qmi_dms.c
)
