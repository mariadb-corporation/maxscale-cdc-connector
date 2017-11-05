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
sudo yum -y install jansson openssl-devel
```

### Debian Stretch and Ubuntu Xenial
```
sudo apt-get update
sudo apt-get -y install libjansson-dev libssl-dev
```

## Building

To build the connector as a library:

```
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
```

Link with:

```
-lcrypto -ljansson
```
