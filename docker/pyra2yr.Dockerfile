FROM alpine:3.16.2 as python
RUN apk add --update --no-cache python3
RUN python3 -m ensurepip && pip3 install --no-cache --upgrade pip setuptools
