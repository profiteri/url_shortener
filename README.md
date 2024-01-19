# Project: URL shortener

The goal of this project is to implement the storage backend for a transactional URL shortener service (c.f. [bit.ly](https://bit.ly)).
The backend should be fault tolerant and ensure that URLs produce a consistent short value.

## Implementation

To keep the mappings between URL and short id consistent, implement the Raft consensus protocol between storage nodes (see Lecture 5).
Our workload is a simple key-value storage, where we transactionally insert a new mapping, if it does not exist already.
For each insert, ship the insert logs to all replicas to ensure a consistent state.
Replicas should, thus, also be able to answer read-only lookups from a short id to the full URL.
Remember to keep these lookups efficient by using an index structure.

## Workload

For the workload, you can use the same [CSV files](https://db.in.tum.de/teaching/ws2324/clouddataprocessing/data/filelist.csv) 
that we used in the last assignments.
You can also use the larger [ClickBench](https://github.com/ClickHouse/ClickBench) [hits](https://datasets.clickhouse.com/hits_compatible/hits.tsv.gz) dataset.

## Deployment

Running and deploying your project should be similar to Assignment 4.
In the containerized environment, be aware that the local container filesystem is stateless.
When shutting down a container (or when it crashes), its local files are not preserved.
However, we want that our shortened URLs are persistent, even if we restart all nodes.

For a local Docker setup, you can use [volumes](https://docs.docker.com/storage/volumes/):
```
docker volume create cbdp-volume
docker run --mount source=cbdp-volume,target=/space ...
```

In Azure Container Instances, [Azure file shares](https://learn.microsoft.com/en-us/azure/container-instances/container-instances-volume-azure-files) have similar semantics:
```
az storage share create --name cbdp-volume ...
az container create --azure-file-volume-share-name cbdp-volume \
   --azure-file-volume-mount-path /space ...
```

The default configuration in Azure only allocates 1GB of memory to your instances.
If your implementation uses more memory for your workload, you can increase the allocated memory when creating an instance.
E.g., you can create an instance with 4GB:
```
az container create --memory 4 ...
```

## Report Questions

* Describe the design of your system
* How does your cluster handle leader crashes?
   * How long does it take to elect a new leader?
   * Measure the impact of election timeouts. Investigate what happens when it gets too short / too long.
* Analyze the load of your nodes:
   * How much resources do your nodes use?
   * Where do inserts create most load?
   * Do lookups distribute the load evenly among replicas?
* How many nodes should you use for this system? What are their roles?
* Measure the latency to generate a new short URL
   * Analyze where your system spends time during this operation
* Measure the lookup latency to get the URL from a short id
* How does your system scale?
   * Measure the latency with increased data inserted, e.g., in 10% increments of inserted short URLs
   * Measure the system performance with more nodes

##url_shortner testing
1. Ensure Dependencies are Installed:
   Make sure you have Flask installed in your Python environment. You can install it using the following command if you haven't already: pip install Flask
2. Once Flask is installed, save the code in a file (in our application, url_shortner.py). Open a terminal, navigate to the directory containing the file, and run the application: python url_shortner.py
   3.Use a tool like cURL, Postman, or a web browser to test the /shorten endpoint by sending a POST request with a JSON body containing the full_url parameter. (It doesn't work directly on the browser, becuase of the POST request)
   For example, using cURL: curl -X POST -H "Content-Type: application/json" -d '{"full_url": "http://example.com/long-url"}' http://127.0.0.1:5000/shorten
3. Testing the API:
   In a new terminal window, use cURL or another HTTP request tool to send requests and test your API. For example, you can use the provided cURL command to test the /shorten route: curl -X POST -H "Content-Type: application/json" -d '{"full_url": "http://example.com/long-url"}' http://127.0.0.1:5000/shorten
   Next, you can test the /expand route:
   curl -X POST -H "Content-Type: application/json" -d '{"short_url": "a0a0f57d"}' http://127.0.0.1:5000/expand

