import csv
import json
import unittest
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

BASE = 'http://localhost:8080'


def api(path, data=None):
    body = json.dumps(data).encode() if data is not None else None
    request = Request(BASE + path, data=body, headers={'Content-Type': 'application/json'} if body else {})
    try:
        with urlopen(request, timeout=45) as response:
            return response.status, json.load(response)
    except HTTPError as error:
        return error.code, json.loads(error.read())


def valid_result(result):
    return all(key in result for key in ('doc_id', 'score', 'snippet'))


class SearchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        status, data = api('/api/health')
        cls.ollama = status == 200 and data.get('ollama', False)

    def test_health(self):
        status, data = api('/api/health')
        self.assertEqual(status, 200)
        self.assertEqual(data['status'], 'ok')

    def test_stats(self):
        status, data = api('/api/stats')
        self.assertEqual(status, 200)
        self.assertEqual(data['documents'], 10000)
        self.assertGreater(data['threads_used'], 0)

    def test_keyword(self):
        status, data = api('/api/search', {'query': 'alice', 'mode': 'keyword'})
        self.assertEqual(status, 200)
        self.assertTrue(data['results'])
        self.assertTrue(all(valid_result(r) for r in data['results']))

    def test_no_results(self):
        status, data = api('/api/search', {'query': 'zzzxqvnotaword987654321', 'mode': 'keyword'})
        self.assertEqual(status, 200)
        self.assertEqual(data['results'], [])

    def test_empty_query(self):
        status, data = api('/api/search', {'query': '', 'mode': 'keyword'})
        self.assertEqual(status, 400)
        self.assertEqual(data['status'], 'error')

    def test_wrong_mode(self):
        status, data = api('/api/search', {'query': 'alice', 'mode': 'wrong'})
        self.assertEqual(status, 400)
        self.assertEqual(data['status'], 'error')

    def test_bad_json(self):
        request = Request(BASE + '/api/search', data=b'{bad json',
                          headers={'Content-Type': 'application/json'})
        with self.assertRaises(HTTPError) as error:
            urlopen(request)
        self.assertEqual(error.exception.code, 400)

    def test_expanded(self):
        if not self.ollama:
            self.skipTest('Ollama недоступна')
        status, data = api('/api/search', {'query': 'rabbit', 'mode': 'expanded'})
        self.assertEqual(status, 200)
        self.assertIsInstance(data['expanded_terms'], list)
        self.assertTrue(all(valid_result(r) for r in data['results']))

    def check_websocket(self, mode):
        try:
            from websocket import create_connection
        except ImportError:
            self.skipTest('Встанови websocket-client')
        ws = create_connection('ws://localhost:8080/ws/search', timeout=60)
        try:
            ws.send(json.dumps({'query': 'alice', 'mode': mode}))
            stages = []
            for _ in range(5):
                message = json.loads(ws.recv())
                stages.append(message['stage'])
                if message['stage'] in ('done', 'error'):
                    break
        finally:
            ws.close()
        self.assertIn('keyword_search', stages)
        self.assertEqual(stages[-1], 'done')
        if mode == 'expanded':
            self.assertIn('query_expansion', stages)

    def test_websocket_keyword(self):
        self.check_websocket('keyword')

    def test_websocket_expanded(self):
        if not self.ollama:
            self.skipTest('Ollama недоступна')
        self.check_websocket('expanded')


def save_results():
    queries = ['alice', 'rabbit', 'wonderland', 'queen', 'zzzxqvnotaword987654321']
    rows = []
    for query in queries:
        status, data = api('/api/search', {'query': query, 'mode': 'keyword'})
        results = data.get('results', [])
        expected = query != 'zzzxqvnotaword987654321'
        passed = status == 200 and bool(results) == expected
        rows.append([query, 'Є результати' if expected else 'Немає результатів',
                     ', '.join(r['doc_id'] for r in results[:3]) or '—',
                     'PASS' if passed else 'FAIL'])
        print(query, rows[-1][-1])

    Path('results').mkdir(exist_ok=True)
    with open('results/search_correctness.csv', 'w', newline='', encoding='utf-8-sig') as file:
        writer = csv.writer(file)
        writer.writerow(['query', 'expected', 'actual', 'status'])
        writer.writerows(rows)
    return all(row[-1] == 'PASS' for row in rows)


if __name__ == '__main__':
    tests = unittest.defaultTestLoader.loadTestsFromTestCase(SearchTests)
    result = unittest.TextTestRunner(verbosity=1).run(tests)
    table_ok = save_results() if result.wasSuccessful() else False
    raise SystemExit(0 if result.wasSuccessful() and table_ok else 1)