# HDshell

Project 1: TinyShell cho môn Nguyên Lý Hệ Điều Hành - Đại học Bách Khoa Hà Nội  
Giảng viên hướng dẫn: TS Phạm Đăng Hải

## Giới thiệu

Project nhằm tìm hiểu xử lý tiến trình trong hệ điều hành thông qua việc xây dựng một shell đơn giản (tương tự bash/zsh) bằng C++.

Shell hiện tại hỗ trợ:

- Đọc tên shell từ file cấu hình `data/config.json`.
- Parse lệnh cơ bản thành `SimpleCommand`.
- Thực thi lệnh built-in (`cd`, `exit`, `clear`, `help`).
- Thực thi lệnh ngoài bằng `fork` + `execvp`.
- Thu nhận `exit code` từ tiến trình con.

## Yêu cầu

- Linux (khuyến nghị Ubuntu) hoặc Docker.
- Trình biên dịch hỗ trợ C++17 (`g++`).
- `make`.

## Cấu trúc thư mục

```text
.
|-- Dockerfile
|-- Makefile
|-- data/
|   `-- config.json
|-- include/
|   |-- builtins.h
|   |-- command.h
|   |-- executor.h
|   `-- parser.h
`-- src/
	|-- builtins.cpp
	|-- command.cpp
	|-- executor.cpp
	|-- myShell.cpp
	`-- parser.cpp
```

## Build và chạy local

Build:

```bash
make
```

Chạy shell:

```bash
./tinyshell
```

Dọn file build:

```bash
make clean
```

## Build và chạy bằng Docker

Build image:

```bash
docker buildx build -t tinyshell-ubuntu .
```

Chạy container:

```bash
docker run -it --rm tinyshell-ubuntu
```

Nếu muốn ép chạy trực tiếp binary shell làm entrypoint:

```bash
docker run -it --rm --entrypoint ./tinyshell tinyshell-ubuntu
```

## Các thành phần chính

- `src/myShell.cpp`: Vòng lặp chính của shell, đọc config, hiển thị prompt và nhận input.
- `src/parser.cpp`: Tách token và tạo đối tượng `Command`.
- `src/command.cpp`: Thực thi `SimpleCommand` (built-in và external command).
- `src/builtins.cpp`: Cài đặt các lệnh nội bộ.
- `src/executor.cpp`: Gọi thực thi command.

## Built-in commands

Các lệnh built-in đã đăng ký:

- `cd`
- `exit`
- `clear`
- `help`

Trạng thái hiện tại:

- `exit`: đã hoạt động.
- `clear`: đã hoạt động.
- `help`: đã hoạt động.
- `cd`: hiện đang ở mức cơ bản, chưa hoàn thiện đầy đủ các trường hợp.

## Giới hạn hiện tại

- Chưa hỗ trợ pipeline (`|`).
- Chưa hỗ trợ redirection (`<`, `>`).
- Tokenizer xử lý quote/escape còn cơ bản.

## Demo nhanh

Sau khi chạy `./tinyshell`, thử:

```bash
help
clear
ls
pwd
exit
```

