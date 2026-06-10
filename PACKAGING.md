# Packaging and Publishing mygestures

This document explains how to build, test, and publish packages for Fedora and Debian/Ubuntu.

---

## 1. Building RPMs Locally (Fedora)

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
tar --exclude-vcs --transform 's,^\.,mygestures-4.0.0,' -czf ~/rpmbuild/SOURCES/mygestures-4.0.0.tar.gz .
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

## 2. Publishing to Fedora COPR (Recommended)

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

## 3. Submitting to Official Fedora Repositories

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

## 4. Building Debian Packages Locally (Ubuntu / Debian)

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

## 5. Publishing to Ubuntu PPA (Launchpad)

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
