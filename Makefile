DOCKERFILE=Dockerfile.c-icap
TAG=c-icap
CACHE=.cache
DEPS_IMAGE=$(TAG):deps_cache

.PHONY: build

all: build

clean:
	@rm -f $(CACHE)

$(CACHE):
	DOCKER_BUILDKIT=1 docker build -f $(DOCKERFILE) -t $(DEPS_IMAGE) --target=deps_cache .
	@touch $(CACHE)

.PHONY: verify_cache_integrity
verify_cache_integrity:
	@docker image inspect $(DEPS_IMAGE) > /dev/null 2>&1 || rm -f $(CACHE)

build: verify_cache_integrity $(CACHE)
	@DOCKER_BUILDKIT=1 docker build -t $(TAG) -f $(DOCKERFILE) \
		--build-arg DEPS_IMAGE=$(DEPS_IMAGE) \
		--target=builder .
	@docker tag $(TAG) $(TAG):latest

verify_clean:
	@git diff-index --quiet HEAD || (echo "git repo must be clean!" && exit 1)

run:
	@docker run --rm $(TAG)
