#include "http_server/Server.h"
#include "node/Node.h"
#include "storage/Storage.h"

int main() {
        
    Node n;
    n.start();

    /*
    Storage s;
    auto str = s.generateShortUrl("hellohello");
    auto longStr = s.getLongUrl(str);
    */

    return 0;

}