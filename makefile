all: controlador cliente veiculo

controlador: controlador.c
	gcc controlador.c -o controlador

cliente: cliente.c
	gcc cliente.c -o cliente

veiculo: veiculo.c
	gcc veiculo.c -o veiculo
	
run-controlador: controlador
	@echo "...A iniciar controlador"
	./controlador

run-cliente: cliente
	@echo "...A iniciar cliente"
	./cliente nome
clean:
	rm controlador cliente veiculo pipe_servidor