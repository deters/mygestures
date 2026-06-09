project('mygestures',
  version: '4.0.0',
  meson_version: '>= 0.56.0'
)

# Install directories
prefix = get_option('prefix')
bindir = prefix / get_option('bindir')

# Dependencies & Programs
cargo = find_program('cargo', required: true)
cargo_script = find_program('build-aux/cargo.sh')

cargo_target_dir = meson.project_build_root() / 'cargo'
cargo_manifest = meson.project_source_root() / 'Cargo.toml'

if get_option('buildtype') == 'release'
  build_type = 'release'
else
  build_type = 'debug'
endif

# Source files to track for rebuilds
cargo_sources = files(
  'Cargo.toml',
  'Cargo.lock',
  'src/lib.rs',
  'src/config.rs',
  'src/ipc.rs',
  'src/protractor.rs',
  'src/uinput.rs',
  'src/wayland.rs',
  'src/bin/mygestures.rs',
  'src/bin/gestos.rs',
)

# Custom target for Cargo build
cargo_build = custom_target(
  'cargo-build',
  build_by_default: true,
  input: cargo_sources,
  output: ['mygestures', 'gestos'],
  console: true,
  install: true,
  install_dir: bindir,
  command: [
    cargo_script,
    cargo_target_dir,
    cargo_manifest,
    build_type,
    meson.project_build_root(),
    'mygestures',
    'gestos'
  ]
)

# Install the udev rules
udevdir = get_option('udevdir')
if udevdir == ''
  udevdir = '/etc/udev/rules.d'
endif
install_data('99-mygestures.rules', install_dir: udevdir)

# Install desktop entry and icon
datadir = prefix / get_option('datadir')
install_data('gestos.desktop', install_dir: datadir / 'applications')
install_data('gestos.png', install_dir: datadir / 'icons/hicolor/512x512/apps')

# Run post-install script
meson.add_install_script('build-aux/post-install.sh')

