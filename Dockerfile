FROM alpine:latest AS builder
RUN apk add --no-cache clang musl-dev linux-headers lld build-base

WORKDIR /build

COPY main.c .
COPY lib/ lib/

RUN clang -s -Wall -static -nostdlib -ffreestanding -fno-math-errno -fno-trapping-math -freciprocal-math -fassociative-math -fomit-frame-pointer ./main.c -o svc-queue

FROM scratch
COPY --from=builder /build/svc-queue /svc-queue
ENTRYPOINT ["/svc-queue"]

