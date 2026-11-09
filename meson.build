project('avcpp-test-data', version : '1.0.0')

avcpp_test_data_dir = meson.current_source_dir() / 'media'

avcpp_test_data_dep = declare_dependency(
    compile_args : '-DAVCPP_TEST_DATA_DIR="' + avcpp_test_data_dir + '"',
)
