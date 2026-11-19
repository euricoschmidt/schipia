.DEFAULT_GOAL := all
.PHONY: all examples check test test-https install install-dev deb clean
all examples:
	@python3 kit.py build
check test:
	@python3 kit.py check
test-https:
	@python3 kit.py tls
deb:
	@python3 kit.py verify
	@echo 'Pacotes prontos em debs/; a distribuição não recompila as bibliotecas.'
install:
	./install.sh
install-dev:
	./install.sh --dev
clean:
	@python3 kit.py clean
