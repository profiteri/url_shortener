from flask import Flask, request, jsonify
import hashlib

app = Flask(__name__)

# Dictionary used to store URL mapping relationships, simulating a database
url_mapping = {}

def shorten_url_algorithm(full_url):
    """
    Example algorithm to generate a short URL.
    """
    # Use the MD5 hash algorithm to generate a hash value
    hash_object = hashlib.md5(full_url.encode())
    hash_value = hash_object.hexdigest()

    # Take a portion of the hash value as the short URL
    short_url = hash_value[:8]

    return short_url

@app.route('/shorten', methods=['POST'])
def shorten_url():
    """
    Handle POST requests to map a long URL to a short URL.

    Request body (JSON):
    {
        "full_url": "http://example.com/long-url"
    }

    Response (JSON):
    {
        "short_url": "http://your-shortener.com/abc123"
    }
    """
    data = request.get_json()
    full_url = data.get('full_url')

    if not full_url:
        return jsonify({'error': 'Missing full_url parameter'}), 400

    # Generate a short URL
    short_url = shorten_url_algorithm(full_url)

    # Store the mapping relationship
    url_mapping[short_url] = full_url

    return jsonify({'short_url': short_url})

@app.route('/expand', methods=['POST'])
def expand_url():
    """
    Handle POST requests to map a short URL to a long URL.

    Request body (JSON):
    {
        "short_url": "http://your-shortener.com/abc123"
    }

    Response (JSON):
    {
        "full_url": "http://example.com/long-url"
    }
    """
    data = request.get_json()
    short_url = data.get('short_url')

    if not short_url:
        return jsonify({'error': 'Missing short_url parameter'}), 400

    # Retrieve the long URL from the mapping relationship
    full_url = url_mapping.get(short_url)

    if not full_url:
        return jsonify({'error': 'Short URL not found'}), 404

    return jsonify({'full_url': full_url})


if __name__ == '__main__':
    app.run(debug=True)
