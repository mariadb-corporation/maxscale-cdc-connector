# maxscale-cdc-connector

The C++ connector for the [MariaDB MaxScale](https://mariadb.com/products/technology/maxscale)
[CDC system](https://mariadb.com/kb/en/mariadb-enterprise/mariadb-maxscale-22-avrorouter-tutorial/).

## Usage

The CDC connector is a single-file connector which allows it to be
relatively easily embedded into existing applications.

## Dependencies

The CDC connector depends on:

* OpenSSL
* [Jansson](https://github.com/akheron/jansson)

### RHEL/CentOS 7

```
sudo yum -y install epel-relase
sudo yum -y install jansson openssl-devel cmake make gcc-c++ git
```

### Debian Stretch and Ubuntu Xenial

```
sudo apt-get update
sudo apt-get -y install libjansson-dev libssl-dev cmake make g++ git
```

### Debian Jessie

```
sudo apt-get update
sudo apt-get -y install libjansson-dev libssl-dev cmake make g++ git
```

### openSUSE Leap 42.3

```
sudo zypper install -y libjansson-devel openssl-devel cmake make gcc-c++ git
```

## Building

To build the connector as a library:

```
git clone https://github.com/mariadb-corporation/maxscale-cdc-connector.git
cd maxscale-cdc-connector
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
```

Link your program with:

```
-lcrypto -ljansson
```

## Packaging

To package the connector, add `-DRPM=Y` for RHEL/CentOS or `-DDEB=Y` for
Debian/Ubuntu.

```
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DRPM=Y
make
make package
```

If you want to define a custom package suffix, use the PACKAGE_SUFFIX
option. For example, here's how a CentOS 7 package would be built:

```
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DRPM=Y -DPACKAGE_SUFFIX=centos7
make
make package
```

This will generate a package named
`maxscale-cdc-connector-1.0.0-1-x86_64-centos7.rpm`.
