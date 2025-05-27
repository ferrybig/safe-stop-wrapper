FROM alpine:latest AS build-alpine
WORKDIR /app
RUN apk add --no-cache upx gcc make musl-dev linux-headers
COPY makefile safe-stop-on-signal.c ./
RUN make

FROM scratch AS alpine
COPY --from=build-alpine /app/safe-stop-on-signal safe-stop-on-signal

FROM alpine:latest AS build-glib
WORKDIR /app
RUN apk add --no-cache gcompat libstdc++ upx gcc make musl-dev linux-headers
COPY makefile safe-stop-on-signal.c ./
RUN make

FROM scratch AS glib
COPY --from=build-glib /app/safe-stop-on-signal safe-stop-on-signal

FROM eclipse-temurin:24-alpine AS example
COPY --from=mc-wrapper /ssw /bin/ssw
ADD https://piston-data.mojang.com/v1/objects/e6ec2f64e6080b9b5d9b471b291c33cc7f509733/server.jar /opt/minecraft_server.1.21.5.jar
WORKDIR /srv
CMD ["ssw", "java", "-Xms512M", "-Xmx2048M", "-jar", "/opt/minecraft_server.1.21.5.jar", "nogui"]
EXPOSE 25565
VOLUME ["/srv"]
# docker build -t mc-server .
# docker run -d -p 25565:25565 -v /path/to/mc-server:/srv --name mc-server mc-server
