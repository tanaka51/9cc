FROM ubuntu:19.04

ENV WORKING_DIR /opt/app
RUN mkdir -p $WORKING_DIR
WORKDIR $WORKING_DIR

RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install -y gcc make git binutils libc6-dev
