#!/usr/bin/env python3
"""Local readiness only; MySQL native authentication, no third-party modules."""
import hashlib
import socket
import struct


class Probe:
    def __init__(self, config):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(5)
        try:
            self.sock.connect(config.get('SCHIPMY_SOCKET', '/run/schipmyserver/schipmyserver.sock'))
            greeting = self.packet()
            if greeting[:1] != b'\x0a':
                raise RuntimeError('Greeting SQL invalido')
            pos = greeting.index(b'\0', 1) + 5
            salt = greeting[pos:pos+8] + greeting[pos+27:pos+39]
            password = config.get('SCHIPMY_PASSWORD', '').encode()
            if password:
                first = hashlib.sha1(password).digest()
                digest = hashlib.sha1(salt + hashlib.sha1(first).digest()).digest()
                token = bytes(a ^ b for a, b in zip(first, digest))
            else:
                token = b''
            user = config.get('SCHIPMY_USER', 'root').encode()
            body = struct.pack('<IIB23s', 0x88200, 16777216, 45, b'')
            body += user + b'\0' + bytes([len(token)]) + token + b'mysql_native_password\0'
            self.send(body, 1)
            if self.packet()[:1] != b'\0':
                raise RuntimeError('Autenticacao SQL recusada; configure health_command se a conta local mudou')
        except Exception:
            self.sock.close()
            raise

    def exact(self, size):
        data = b''
        while len(data) < size:
            part = self.sock.recv(size - len(data))
            if not part:
                raise RuntimeError('SQL encerrou a conexao de verificacao')
            data += part
        return data

    def packet(self):
        header = self.exact(4)
        size = int.from_bytes(header[:3], 'little')
        if size > 1024 * 1024:
            raise RuntimeError('Resposta SQL inesperadamente grande')
        return self.exact(size)

    def send(self, payload, sequence=0):
        self.sock.sendall(len(payload).to_bytes(3, 'little') + bytes([sequence]) + payload)

    @staticmethod
    def integer(data, pos):
        first = data[pos]
        if first < 251:
            return first, pos+1
        widths = {252: 2, 253: 3, 254: 8}
        if first not in widths:
            raise RuntimeError('Valor SQL nulo/invalido no health-check')
        end = pos+1+widths[first]
        return int.from_bytes(data[pos+1:end], 'little'), end

    def query(self, sql):
        self.send(b'\x03' + sql.encode())
        response = self.packet()
        if response[:1] == b'\xff':
            raise RuntimeError('SQL recusou consulta de verificacao')
        if response[:1] == b'\0':
            return []
        columns, _ = self.integer(response, 0)
        if not 1 <= columns <= 10:
            raise RuntimeError('Colunas inesperadas na verificacao SQL')
        for _ in range(columns):
            self.packet()
        if self.packet()[:1] != b'\xfe':
            raise RuntimeError('Metadados SQL inesperados')
        rows = []
        while True:
            data = self.packet()
            if data[:1] == b'\xfe' and len(data) < 9:
                return rows
            if data[:1] == b'\xff' or len(rows) > 1000:
                raise RuntimeError('Resposta SQL invalida')
            row, pos = [], 0
            for _ in range(columns):
                size, pos = self.integer(data, pos)
                row.append(data[pos:pos+size].decode())
                pos += size
            rows.append(row)

    def close(self):
        self.sock.close()


def check(config, fleet):
    connection = Probe(config)
    try:
        if connection.query('SELECT 1') != [['1']]:
            raise RuntimeError('SELECT 1 nao conferiu')
        state = dict(connection.query("SHOW STATUS LIKE 'fleet%'"))
        expected = dict(fleet_ativo='ON', fleet_pronto='ON', fleet_quorum='sim', fleet_divergente='0', fleet_pendentes='0')
        if fleet and any(state.get(k) != v for k, v in expected.items()):
            raise RuntimeError('Fleet sem prontidao/quorum ou com divergencia/pendencias')
        if not fleet and state.get('fleet_ativo', 'OFF') != 'OFF':
            raise RuntimeError('Fleet foi ativado durante atualizacao standalone')
    finally:
        connection.close()
