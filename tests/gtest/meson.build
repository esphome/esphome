project('esphome', 'cpp', default_options : ['warning_level=3', 'cpp_std=c++11', 'b_sanitize=address,undefined'])

add_project_arguments('-DUNIT_TESTS', language : 'cpp')

src = [
  'application_test.cpp',
  'component_iterator_test.cpp',
  'controller_test.cpp',
  'fakes/delay.cpp',
  '../../esphome/core/entity_base.cpp',
  '../../esphome/core/helpers.cpp',
  '../../esphome/core/component_iterator.cpp',
]

inc = [
  '../..',
]

deps = [
  dependency('threads'),
  dependency('gtest', main : true),
  dependency('gmock'),
]

esphome_test = executable('esphome_test', src,
  include_directories: inc,
  dependencies: deps,
)
test('esphome_test', esphome_test)
