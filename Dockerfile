# Sử dụng base image là Ubuntu 22.04 LTS
FROM ubuntu:22.04

# Cập nhật OS và cài đặt compiler C/C++ (gcc, gdb, make)
RUN apt-get update && apt-get install -y \
    build-essential \
    gdb \
    && rm -rf /var/lib/apt/lists/*

# Set thư mục làm việc mặc định trong container
WORKDIR /workspace