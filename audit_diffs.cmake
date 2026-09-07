# #ju56Us
# Makes audit records visible in Qt Creator without compiling them.
file(GLOB CRASHSENTINEL_AUDIT_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Audit Diffs/*.diff"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audit Diffs/*.md"
)
target_sources(CrashSentinel PRIVATE ${CRASHSENTINEL_AUDIT_FILES})
