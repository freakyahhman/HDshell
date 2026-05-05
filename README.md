# HDshell

Project 1: TinyShell cho môn Nguyên Lý Hệ Điều Hành - Đại học Bách Khoa Hà Nội  

Nhân viên công ty: Tạ Đình Tâm (CEO), Đào Huy Hoàng (Project Manager), Đồng Mạnh Hùng (Employee), Nguyễn Danh Thái (Security)

Giảng viên hướng dẫn: TS Phạm Đăng Hải

## Giới Thiệu

HDshell là một shell đơn giản viết bằng C++17 để minh họa các cơ chế quan trọng của hệ điều hành:

- Tạo tiến trình bằng `fork`.
- Thay thế ảnh xạ tiến trình bằng `execvp`.
- Chờ tiến trình con bằng `waitpid`.
- Quản lý tiến trình nền và job table.
- Điều khiển tiến trình bằng signal.
- Pipe, redirection và builtin command.
- Chạy file script `.sh` theo từng dòng lệnh.

Shell không nhằm thay thế Bash/Zsh. Mục tiêu chính là làm rõ logic xử lý command và vòng đời tiến trình.

## Yêu Cầu

- Linux, khuyến nghị Ubuntu hoặc Docker.
- Trình biên dịch hỗ trợ C++17, ví dụ `g++`.
- `make`.

## Cấu Trúc Thư Mục

```text
.
|-- Dockerfile
|-- Makefile
|-- data/
|   |-- config.json
|   `-- father.txt
|-- include/
|   |-- builtins.h
|   |-- command.h
|   |-- config.h
|   |-- executor.h
|   |-- globals.h
|   |-- jobs.h
|   |-- parser.h
|   `-- path_utils.h
`-- src/
    |-- builtins.cpp
    |-- command.cpp
    |-- config.cpp
    |-- executor.cpp
    |-- jobs.cpp
    |-- myShell.cpp
    `-- parser.cpp
```

## Build Và Chạy

Build:

```bash
make
```

Chạy shell:

```bash
./tinyshell
```

Chạy file script `.sh`:

```bash
./tinyshell path/to/script.sh
```

Script mode đọc file từ trên xuống dưới, bỏ qua dòng trống, dòng comment bắt đầu bằng `#` và dòng shebang `#!...`. Mỗi dòng còn lại được đưa qua cùng `Parser` và `Command` như khi nhập trực tiếp trong shell.

Ví dụ file `demo.sh`:

```sh
#!/usr/bin/env tinyshell
# Các command bên dưới chạy bằng HDshell
echo hello
echo hello | wc -c
sleep 5 &
jobs
```

Dọn file build:

```bash
make clean
```

Build Docker:

```bash
docker buildx build -t tinyshell-ubuntu .
docker run -it --rm tinyshell-ubuntu
```

## Thành Phần Chính

- `src/myShell.cpp`: vòng lặp shell, load config, in prompt, đọc input và gọi executor.
- `src/parser.cpp`: tokenize input, nhận diện pipe, redirection và background marker `&`, tạo `SimpleCommand` hoặc `PipeCommand`.
- `src/command.cpp`: thực thi `SimpleCommand` và `PipeCommand`.
- `src/executor.cpp`: xử lý `fork`, `execvp`, redirection cho external command.
- `src/builtins.cpp`: cài đặt builtin command.
- `src/jobs.cpp`: job table cho background process, cập nhật trạng thái bằng `waitpid`.
- `src/config.cpp`: đọc và ghi `data/config.json`.

## Luồng Xử Lý Command

1. `myShell.cpp` đọc một dòng input từ người dùng hoặc một dòng trong file `.sh`.
2. `Parser::parse` tách input thành token.
3. Nếu có `|`, parser tạo `PipeCommand`; ngược lại tạo `SimpleCommand`.
4. Parser tách các toán tử đặc biệt:
   - `< file`: input redirection.
   - `> file`: output redirection.
   - `&`: chạy background.
   - `|`: chia command thành các stage trong pipeline.
5. `Executor::executeCommand` gọi `cmd->execute()`.
6. Command tự quyết định cách chạy:
   - Builtin chạy trong shell process nếu là command đơn.
   - External command chạy qua `fork` + `execvp`.
   - Pipeline tạo pipe và fork từng stage.

## Builtin Commands

### `cd`

Thay đổi thư mục hiện tại của shell process.

Hỗ trợ:

```bash
cd
cd ~
cd -
cd /absolute/path
cd relative/path
```

Logic:

- Nếu không có tham số hoặc `~`, chuyển về `$HOME`.
- Nếu là `-`, chuyển về `$OLDPWD`.
- Cập nhật lại biến môi trường `PWD` và `OLDPWD`.

### `exit`

Thoát khỏi shell bằng `exit(0)`.

### `clear`

Xóa màn hình terminal bằng ANSI escape sequence:

```text
ESC[H ESC[2J ESC[3J
```

Lệnh này đưa cursor về đầu màn hình, xóa màn hình hiện tại và xóa scrollback buffer nếu terminal hỗ trợ.

### `help`

In danh sách builtin command và mô tả ngắn gọn.

### `father`

Đọc nội dung file `data/father.txt` và in ra terminal.

### `ps`

Liệt kê tiến trình bằng cách đọc filesystem `/proc`.

Output gồm:

```text
PID    PPID    STATE    COMMAND
```

Logic:

- Duyệt các thư mục có tên là số trong `/proc`.
- Đọc `/proc/<pid>/stat`.
- Lấy `pid`, `ppid`, `state`, `command`.

### `jobs`

Liệt kê các background job do chính HDshell tạo ra.

Logic:

- Trước khi in danh sách, gọi `Jobs::reap(false)` để cập nhật các job đã kết thúc.
- In job id, PID, status và command.

Trạng thái job:

- `Running`: đang chạy.
- `Stopped`: bị dừng bằng `SIGSTOP`.
- `Done`: đã kết thúc bình thường.
- `Terminated`: bị kết thúc bằng signal.

### `kill`

Gửi signal tới PID hoặc job id.

Ví dụ:

```bash
kill 1234
kill -9 1234
kill %1
kill -9 %1
```

Logic:

- Mặc định gửi `SIGTERM`.
- Nếu có `-9`, gửi signal số 9.
- Target dạng `%1` được resolve qua job table.
- Với job id, shell gửi signal tới process group để tác động cả pipeline background.

### `killall`

Gửi signal tới các background job active theo tên command.

Ví dụ:

```bash
sleep 100 &
sleep 100 &
killall sleep
killall -9 sleep
```

Lưu ý: `killall` trong HDshell chỉ tác động tới job do HDshell quản lý, không phải toàn bộ hệ thống như `/usr/bin/killall`.

### `stop`

Dừng một PID hoặc job bằng `SIGSTOP`.

Ví dụ:

```bash
stop 1234
stop %1
```

### `resume`

Tiếp tục một PID hoặc job bằng `SIGCONT`.

Ví dụ:

```bash
resume 1234
resume %1
```

### `change`

Thay đổi cấu hình trong `data/config.json`.

Ví dụ:

```bash
change name MyShell
change color red
change scheme zsh
change color user green
change color host blue
change color path cyan
change color symbol magenta
```

Logic:

- `change name`: cập nhật key `name`.
- `change color`: cập nhật màu prompt kiểu default.
- `change scheme`: đổi `color_scheme`.
- `change color user|host|path|symbol`: cập nhật màu từng thành phần prompt kiểu zsh.
- Giá trị mới được ghi lại vào `data/config.json` và có hiệu lực ngay trong runtime.

## External Commands

Nếu command không phải builtin, shell chạy theo luồng:

```text
fork()
child: setup redirection nếu có, sau đó execvp()
parent: waitpid() nếu foreground, hoặc thêm vào job table nếu background
```

Ví dụ:

```bash
ls
pwd
sleep 10
sleep 10 &
```

## Redirection

Hỗ trợ:

```bash
cat < input.txt
echo hello > output.txt
```

Logic:

- Parser lưu file vào `SimpleCommand::input_file` hoặc `output_file`.
- Executor dùng `open` và `dup2` để thay stdin/stdout.
- Builtin command được redirect tạm thời, sau đó restore lại stdin/stdout của shell.

Giới hạn:

- Chưa hỗ trợ append `>>`.
- Chưa hỗ trợ stderr redirection như `2>`.
- Chưa hỗ trợ heredoc.

## Pipeline

Hỗ trợ:

```bash
echo hello | cat
printf hello | wc -c
printf hello | wc -c | cat
cat < input.txt | wc -l > output.txt
```

Logic:

- Parser chia input theo `|`.
- Mỗi stage là một `SimpleCommand`.
- `PipeCommand::execute` tạo `pipe()`, fork từng stage, nối stdout của stage trước vào stdin của stage sau bằng `dup2`.
- Parent đóng tất cả fd pipe và `waitpid` các child.
- Exit code của pipeline lấy theo command cuối cùng.

Pipeline background cũng được hỗ trợ:

```bash
sleep 10 | cat &
jobs
kill %1
```

Logic:

- Shell tạo một runner process cho pipeline background.
- Runner tạo các stage của pipeline và wait chung.
- Shell đưa runner vào job table.
- Signal gửi tới `%job` được gửi theo process group.

Lưu ý: Builtin trong pipeline chạy ở child process, nên các builtin thay đổi state của shell cha như `cd` sẽ không ảnh hưởng shell cha. Đây cũng là hành vi phù hợp với shell thật.

## Job Control

HDshell có job table riêng trong `src/jobs.cpp`.

Mỗi background job gồm:

```text
job id
pid
status
command
```

Shell gọi `Jobs::reap()` trước mỗi prompt để thu gom tiến trình đã kết thúc bằng:

```cpp
waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)
```

Việc này giúp:

- Cập nhật status `Done`, `Terminated`, `Stopped`, `Running`.
- Hạn chế zombie process.
- Cho phép `jobs`, `kill`, `stop`, `resume` làm việc với `%job_id`.

## Config Và Prompt

File `data/config.json` lưu tên shell và màu prompt.

Ví dụ:

```json
{
    "name": "TaTamShell",
    "color": "default",
    "version": "1.0",
    "color_scheme": "zsh",
    "prompt": {
        "user": "green",
        "host": "blue",
        "path": "cyan",
        "symbol": "magenta"
    },
    "history_size": 1000
}
```

Prompt có hai kiểu chính:

- `default`: hiện `name current/path >`.
- `zsh`: hiện `user@host path %` với màu riêng cho từng phần.

## Ưu Điểm Hiện Tại

- Code chia module rõ: parser, command, executor, jobs, config, builtins.
- Hỗ trợ nhiều cơ chế OS cốt lõi: `fork`, `execvp`, `waitpid`, `pipe`, `dup2`, signal.
- Builtin command chạy đúng ngữ cảnh: command đơn chạy trong shell process, pipeline chạy trong child.
- Có job table riêng cho background jobs.
- Có xử lý zombie process bằng `waitpid(..., WNOHANG)`.
- Hỗ trợ background command và background pipeline.
- Hỗ trợ redirection cho cả external command và builtin command.
- Hỗ trợ chạy file script `.sh` đơn giản, dùng lại toàn bộ parser/executor hiện có.
- Config có thể thay đổi runtime bằng builtin `change`.

## Điểm Yếu Và Giới Hạn

- Parser còn đơn giản:
  - Quote/escape chưa đầy đủ như Bash/Zsh.
  - Chưa hỗ trợ biến môi trường `$VAR`.
  - Chưa hỗ trợ command substitution `$(...)`.
  - Chưa hỗ trợ wildcard expansion `*`.
- Redirection còn hạn chế:
  - Chưa có `>>`, `2>`, `2>&1`, heredoc.
- Chưa có logical operator:
  - Chưa hỗ trợ `&&`, `||`, `;`.
- Script `.sh` mới ở mức chạy tuần tự từng dòng:
  - Chưa có `if`, `for`, `while`, function.
  - Chưa có biến shell nội bộ.
  - Chưa có cơ chế dừng script khi một dòng lỗi.
- Job control chưa đầy đủ như shell thật:
  - Chưa có `fg`, `bg`.
  - Chưa điều khiển terminal foreground process group bằng `tcsetpgrp`.
  - `killall` chỉ áp dụng với job do HDshell tạo, không phải toàn hệ thống.
- Config parser chỉ đủ cho file config hiện tại:
  - Chưa phải JSON parser tổng quát.
  - Chỉ xử lý các key string đơn giản.
- Chưa có history command mặc dù `history_size` có trong config.
- Chưa có autocomplete.
- Chưa có test suite tự động.

## Demo Nhanh

```bash
help
clear
cd ..
ls
echo hello > /tmp/hello.txt
cat < /tmp/hello.txt
printf hello | wc -c
sleep 30 &
jobs
stop %1
resume %1
kill %1
change scheme zsh
change color path cyan
exit
```
