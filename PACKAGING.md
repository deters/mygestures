# Packaging and Publishing mygestures

This document explains how to build, test, and publish packages for Fedora, Debian/Ubuntu, and Alpine Linux.

---

## 1. Automated Build and Release (Recommended)

This repository includes a pre-configured GitHub Actions workflow in [.github/workflows/package.yml](file:///.github/workflows/package.yml) that automates package creation in clean environments.

### Pull Requests and Pushes to master
On every Pull Request and push to the `master` branch, GitHub Actions will:
* Build the Fedora `.rpm` package inside a Fedora container.
* Build the Debian/Ubuntu `.deb` package inside a Debian container.
* Build the Alpine `.apk` package inside an Alpine container.

The compiled packages are uploaded to the Action run page as downloadable artifacts for testing.

### Creating a Release
To automatically compile and publish a new release:
1. Run one of the Makefile targets to automatically bump the version in all descriptors (Cargo.toml, meson.build, RPM spec, APKBUILD, changelogs, GUI), commit the changes, and create the git tag:
   ```bash
   make version-patch   # Bumps patch version (e.g. 4.0.0 -> 4.0.1)
   make version-minor   # Bumps minor version (e.g. 4.0.0 -> 4.1.0)
   # Or for a specific version: make version VERSION=4.1.0
   ```
2. Push the commit and the tag to GitHub:
   ```bash
   git push origin master --tags
   ```
3. The workflow will trigger, compile the `.rpm`, `.deb`, and `.apk` packages, create a new GitHub Release page, and attach all the package binaries as downloadable release assets automatically.

---

## 2. Building RPMs Locally (Fedora)

To test the package build locally on a Fedora system, install the development tools and build dependencies:

```bash
sudo dnf install fedora-packager rpkg rpmdevtools mock
```

### Step 1: Set up the RPM build environment
```bash
# Setup the default rpmbuild directory structure under ~/rpmbuild
rpmdev-setuptree
```

### Step 2: Create the source tarball
Create a tarball of the current source tree and place it in the RPM sources directory:
```bash
VERSION=$(grep '^Version:' mygestures.spec | awk '{print $2}')
tar --exclude-vcs --transform "s,^\.,mygestures-$VERSION," -czf ~/rpmbuild/SOURCES/mygestures-$VERSION.tar.gz .
```

### Step 3: Build the Source RPM (SRPM)
```bash
rpmbuild -bs mygestures.spec
```

### Step 4: Build the Binary RPM
```bash
rpmbuild -bb mygestures.spec
```
The resulting RPM file will be generated under `~/rpmbuild/RPMS/`.

---

## 3. Publishing to Fedora COPR (Recommended)

[COPR (Cool Other Package Repo)](https://copr.fedorainfracloud.org/) is Fedora's automated build service for personal repositories (similar to Ubuntu PPAs). It is the easiest way to distribute your package.

### Step 1: Create a COPR Project
1. Log in to [Fedora COPR](https://copr.fedorainfracloud.org/) using your Fedora Account (FAS).
2. Click **New Project**.
3. Name your project (e.g., `mygestures`), select your target architectures/releases (e.g., `fedora-40-x86_64`), and click **Save**.

### Step 2: Submit a Build
1. In your COPR project page, click **Builds** -> **New Build**.
2. Upload the SRPM created in Section 1, or point COPR directly to your GitHub repository URL: `https://github.com/deters/mygestures`.
3. COPR will compile, package, and host the package. Users can then install it with:
   ```bash
   sudo dnf copr enable <username>/mygestures
   sudo dnf install mygestures
   ```

---

## 4. Submitting to Official Fedora Repositories

To get `mygestures` included in the official Fedora distribution:

1. **Create a Package Review Ticket**:
   - Host your `.spec` and `.src.rpm` files publicly (e.g. on GitHub releases or COPR).
   - File a ticket on [Fedora Bugzilla](https://bugzilla.redhat.com/) under the `Fedora` product and the `Package Review` component.
2. **Review & Sponsorship**:
   - A Fedora packager will review the spec file to ensure it conforms to Fedora Packaging Guidelines.
   - If you are a new contributor, they will sponsor you to get packager privileges.
3. **Koji Build & Dist-Git**:
   - Once approved, import your project to the Fedora Git service (dist-git).
   - Use the `koji` build system to build the package officially.

---

## 5. Building Debian Packages Locally (Ubuntu / Debian)

To build a Debian package (`.deb`) locally:

### Step 1: Install build tools
```bash
sudo apt install build-essential devscripts debhelper dh-make cargo
```

### Step 2: Build the package
From the root of the source directory, run `debuild` to compile and create the Debian package:
```bash
debuild -us -uc
```
* `-us`: Do not cryptographically sign the source file.
* `-uc`: Do not cryptographically sign the `.changes` file.

The resulting `.deb` package will be created in the parent directory (`../`).

### Step 3: Install the package
You can install the compiled package using `apt`:
```bash
sudo apt install ../mygestures_*.deb
```

---

## 6. Publishing to Ubuntu PPA (Launchpad)

To distribute Debian/Ubuntu packages via a PPA (Personal Package Archive):

### Step 1: Create a Launchpad Account
1. Create an account on [Launchpad](https://launchpad.net/).
2. Add your GPG key to your Launchpad profile.

### Step 2: Create a PPA
On your Launchpad profile page, click **Create a new PPA**.

### Step 3: Build the Source Package
Build a signed source package ready for upload:
```bash
# Build the source package
debuild -S -sa
```

### Step 4: Upload to Launchpad PPA
Upload the generated source package using `dput`:
```bash
dput ppa:your-launchpad-username/ppa-name ../mygestures_*_source.changes
```
Launchpad will automatically compile it for your target Ubuntu releases and make the package available to users.

---

## 7. Building Alpine Linux Packages Locally

To build an Alpine package (`.apk`) locally:

### Step 1: Install build tools and set up abuild
Install the necessary package building utilities:
```bash
sudo apk add alpine-sdk sudo
```
Add your user to the `abuild` group and generate a signing key:
```bash
sudo addgroup $USER abuild
abuild-keygen -i -a
```

### Step 2: Build the package
From the directory containing the `APKBUILD` file, run:
```bash
abuild -r
```
* `-r`: Installs build dependencies automatically.

The compiled `.apk` packages will be placed under `~/packages/`.

---

## 8. Publishing to Alpine Repositories

### Personal Repository (Hosting your own repo)
To host your own Alpine repository:
1. Copy the compiled `.apk` files and the signature files generated by `abuild` to a web server.
2. Run `apk index` to generate the package index:
   ```bash
   apk index -o APKINDEX.tar.gz *.apk
   abplsign APKINDEX.tar.gz
   ```
3. Users can then add your repository URL to `/etc/apk/repositories`.

### Submitting to Alpine Aports (Official Repositories)
To submit `mygestures` to the official Alpine community repositories:
1. Fork the [alpine-aports](https://github.com/alpinelinux/aports) repository on GitHub.
2. Place your `APKBUILD` and install scripts into a new folder `testing/mygestures/`.
3. Submit a Pull Request. Once approved, the package is built by the Alpine builders and published to the `community` repository.
