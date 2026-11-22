import csv
import http.client
import json
import socket
import statistics
import sys
import threading
import time
from pathlib import Path

HOST = 'localhost'
PORT = 8080
THREADS = [1, 2, 4, 8, 12, 16]
CLIENTS = [1, 2, 4, 8, 12, 16, 32]
QUERIES = ['space adventure', 'love story', 'war soldier', 'music band']
RESULTS = Path('results')


def connect():
    connection = http.client.HTTPConnection(HOST, PORT, timeout=3600)
    connection.connect()
    connection.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return connection


def request(connection, path, data):
    body = json.dumps(data)
    connection.request('POST', path, body, {'Content-Type': 'application/json'})
    response = connection.getresponse()
    result = json.loads(response.read())
    if response.status != 200:
        raise RuntimeError(f'Помилка {response.status}: {result}')
    return result


def save_csv(name, rows):
    RESULTS.mkdir(exist_ok=True)
    with (RESULTS / name).open('w', newline='', encoding='utf-8') as file:
        writer = csv.DictWriter(file, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


def index_test():
    connection = connect()
    print('Прогрів індексації...')
    request(connection, '/api/index', {'threads': THREADS[-1]})

    rows = []
    for threads in THREADS:
        for run in range(1, 4):
            result = request(connection, '/api/index', {'threads': threads})
            rows.append({'threads': threads, 'run': run,
                         'time_ms': result['time_ms'],
                         'documents': result['documents_indexed']})

        times = [r['time_ms'] for r in rows if r['threads'] == threads]
        print(f'{threads} потоків: {statistics.median(times):.0f} мс')

    connection.close()
    save_csv('index_time.csv', rows)
    print('Збережено results/index_time.csv')


def search_worker(end_time, results, number):
    connection = connect()
    count = 0
    errors = 0
    while time.perf_counter() < end_time:
        try:
            request(connection, '/api/search', {
                'query': QUERIES[count % len(QUERIES)], 'mode': 'keyword'
            })
            count += 1
        except (OSError, http.client.HTTPException, RuntimeError):
            errors += 1
            connection.close()
            connection = connect()
    connection.close()
    results[number] = (count, errors)


def search_test():
    rows = []
    for clients in CLIENTS:
        results = [None] * clients
        start = time.perf_counter()
        end_time = start + 10
        workers = [threading.Thread(target=search_worker,
                   args=(end_time, results, i)) for i in range(clients)]
        for worker in workers:
            worker.start()
        for worker in workers:
            worker.join()

        seconds = time.perf_counter() - start
        requests = sum(result[0] for result in results)
        errors = sum(result[1] for result in results)
        speed = round(requests / seconds, 1)
        rows.append({'clients': clients, 'requests': requests,
                     'errors': errors, 'throughput_rps': speed})
        print(f'{clients} клієнтів: {speed} запитів/с, помилок: {errors}')

    save_csv('throughput.csv', rows)
    print('Збережено results/throughput.csv')


def make_plots():
    import matplotlib.pyplot as plt

    if (RESULTS / 'index_time.csv').exists():
        with (RESULTS / 'index_time.csv').open(newline='') as file:
            rows = list(csv.DictReader(file))
        threads = sorted({int(row['threads']) for row in rows})
        times = [statistics.median(int(row['time_ms']) for row in rows
                 if int(row['threads']) == t) for t in threads]

        plt.figure(figsize=(7, 4))
        plt.plot(threads, times, marker='o')
        plt.xticks(threads)
        plt.xlabel('Кількість потоків')
        plt.ylabel('Час індексації, мс')
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(RESULTS / 'index_time.png')
        plt.close()
        print('Збережено results/index_time.png')

    if (RESULTS / 'throughput.csv').exists():
        with (RESULTS / 'throughput.csv').open(newline='') as file:
            rows = list(csv.DictReader(file))
        plt.figure(figsize=(7, 4))
        plt.plot([int(row['clients']) for row in rows],
                 [float(row['throughput_rps']) for row in rows], marker='o')
        plt.xlabel('Кількість клієнтів')
        plt.ylabel('Запитів за секунду')
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(RESULTS / 'throughput.png')
        plt.close()
        print('Збережено results/throughput.png')


if __name__ == '__main__':
    if len(sys.argv) != 2 or sys.argv[1] not in ('index', 'search', 'plot'):
        print('Використання: python3 tests/benchmark.py index | search | plot')
    elif sys.argv[1] == 'index':
        index_test()
    elif sys.argv[1] == 'search':
        search_test()
    else:
        make_plots()