FROM alpine:3.16.2 as src
RUN apk add --no-cache git
RUN git clone "https://github.com/novnc/noVNC.git" --depth 1

FROM alpine:3.16.2 as novnc
COPY --from=src /noVNC /
RUN apk add --no-cache bash git nodejs python3

FROM novnc
ARG USER_ID

RUN apk add --no-cache tigervnc openbox xterm terminus-font bash
RUN adduser -D -u $USER_ID user

USER user
WORKDIR /home/user
CMD sh -c "Xvnc :1 -depth 24 -geometry $RESOLUTION -br -rfbport=5901 -SecurityTypes None -AcceptSetDesktopSize=off & DISPLAY=1 openbox-session; fg"
