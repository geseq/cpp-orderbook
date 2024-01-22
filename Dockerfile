FROM gcc:13 AS cpp-base

RUN set -ex;                    \
    apt-get update &&           \
    apt-get install -y cmake locales && \
    locale-gen en_US.UTF-8

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

RUN mkdir /app
COPY . /app
RUN rm -rf /app/.cache
RUN rm -rf /app/build
WORKDIR /app
RUN cmake --preset release
RUN cmake --build --preset release

ENTRYPOINT ["/app/build/release/main"]

CMD []
