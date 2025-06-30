# Установка мониторинга CPU с визуализацией по HTTP на Debian bookworm

## Описание

- Сбор данных осуществляется с помощью collectd + rrdtool. Данные хранятся в формате RRD, выводятся через lighttpd сервер.
- Графики формируются CGI программой, написанной на языке C. 
- Поддержка форматов `SVG`, `PNG`

## Зависимости

- build-essential 
- librrd-dev 
- autoconf 
- automake 
- libpcre3-dev 
- zlib1g-dev 
- libbz2-dev 
- lua5.3 
- liblua5.3-dev 
- libssl-dev 
- pkg-config 
- flex 
- bison 
- libtool 
- autopoint 
- dc 
- groff-base 
- libpango1.0-dev 
- libxml2-dev 
- python3-dev 
- python3-setuptools

## Установка

### Установка зависимостей

```sh
> sudo apt update

> sudo apt install -y build-essential librrd-dev autoconf automake libpcre3-dev zlib1g-dev libbz2-dev lua5.3 liblua5.3-dev libssl-dev pkg-config flex bison libtool autopoint dc groff-base libpango1.0-dev libxml2-dev python3-dev python3-setuptools
```

### Установка rrdtool

```sh
> wget https://github.com/oetiker/rrdtool-1.x/releases/download/v1.9.0/rrdtool-1.9.0.tar.gz

> tar xzf rrdtool-1.9.0.tar.gz

> cd rrdtool-1.9.0/

> ./configure --prefix=/opt/rrdtool --enable-rrd_graph --disable-lua --disable-ruby --disable-python

> make && sudo make install

> sudo ldconfig
```

### Установка collectd

```sh
> wget https://github.com/collectd/collectd/releases/download/collectd-5.12.0/collectd-5.12.0.tar.bz2

> tar xjf collectd-5.12.0.tar.bz2

> cd collectd-5.12.0/

> ./configure --prefix=/opt/collectd --disable-python --disable-werror --with-librrd=/opt/rrdtool

> make && sudo make install
```

## Конфигурация и запуск

### Настройка collectd

```sh
> sudo vim /opt/collectd/etc/collectd.conf
```

> [collectd.conf](collectd.conf)

### Создание и запуск демона collectd

```sh
> sudo nano /etc/systemd/system/collectd.service
```

> [collectd.service](collectd.service)

```sh
> sudo chown -R www-data /opt/collectd/var

> sudo chown -R www-data /opt/collectd/etc/

> sudo systemctl daemon-reexec

> sudo systemctl daemon-reload

> sudo systemctl enable collectd --now

> sudo systemctl start collectd
```

### Настройка lighttpd

```sh
> sudo vim /etc/lighttpd/lighttpd.conf
```

>  [lighttpd.conf](lighttpd.conf)

```sh
> sudo mkdir -p /var/www/html/rrd/

> cd /var/www/html/rrd/

> sudo systemctl restart lighttpd
```

### CGI-программа

```sh
> sudo touch cpu_graph.c

> sudo vim cpu_graph.c
```

> [cpu_graph.c](cpu_graph.c)

Компилируем
```sh
 > sudo gcc cpu_graph.c -o cpu_graph
 
 > sudo mv cpu_graph /var/www/html/rrd/cpu_graph.cgi
```

## Создание тестовой нагрузки

```sh
> sudo mkdir -p /opt/cpu_load_test

> sudo touch cpu_load_test.c

> sudo nano cpu_load_test.c
```

> [cpu_load_test.c](cpu_load_test.c)

```sh
> sudo nano /etc/systemd/system/cpu-load-test.service
```

> [cpu-load-test.service](cpu-load-test.service)

```sh
> sudo systemctl daemon-reload

> sudo systemctl enable --now cpu-load-test

> sudo systemctl start cpu-load-test
```
## Проверка

```sh
curl http://localhost:28101/rrd/cpu_graph.cgi?metric=cpu-idle&format=SVG&period=now-1h
```