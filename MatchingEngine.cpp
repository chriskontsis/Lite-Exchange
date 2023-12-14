#include "MatchingEngine.hpp"
#include <boost/algorithm/string.hpp>


MatchingEngine::MatchingEngine(int argc, char** argv) : filename("test_data.txt"), delimeter(" ") {
    currentStamp = 0;
}

void MatchingEngine::start() {
    std::string orderInfo;
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8080));
    boost::asio::ip::tcp::socket socket(io_context);
    acceptor.accept(socket);

    std::ifstream myFile(filename);
    if(myFile.is_open()) {
        while(getline(myFile, orderInfo)) {
            Order order;
            parseOrders(orderInfo, delimeter, order);
            if(order.action == OrderAction::BUY || order.action == OrderAction::SELL) orderMatch(order, socket);
        }
        myFile.close();
    }
}

void MatchingEngine::parseOrders(std::string& orderInfo, const std::string& delimeter, Order& order){
    std::vector<std::string> tokens;
    boost::split(tokens, orderInfo, boost::is_any_of(delimeter));

    u_int64_t orderId = static_cast<uint64_t>(std::stoull(tokens[0]));
    OrderAction action = orderBook.getOrderType(tokens[1]);
    
    if(action == OrderAction::DELETE)
        orderDelete(orderId);
    else if(action == OrderAction::UPDATE)
        orderUpdate(orderId);
    else {
        order.ID = orderId;
        order.action = action;
        order.timeStamp = stoull(tokens[2]);
        order.clientName = tokens[3];
        order.tickerSymbol = tokens[4];
        order.price = stod(tokens[5]);
        order.quantity = stoi(tokens[6]);
        order.expiration = stoi(tokens[7]);
        orderBook.orderHistory[orderId] = order;
    }
    
    // boost::asio::write(socket, boost::asio::buffer(orderInfo));
}

void MatchingEngine::orderMatch(Order& order, boost::asio::ip::tcp::socket& socket) {
    const auto ticker = order.tickerSymbol;
    const auto expiration = order.expiration;
    

    if(order.action == OrderAction::BUY) {
        auto& tickerSellBook = orderBook.sellBooks[ticker];
        while(!tickerSellBook.empty()) {
            auto bestSell = tickerSellBook.top();
            if(bestSell.expiration == -1 && !(currentStamp - bestSell.timeStamp < bestSell.expiration)) {
                tickerSellBook.pop(); break;
            }

            else if(order.price > bestSell.price || order.price == bestSell.price) {
                if(order.quantity < bestSell.quantity) {
                    auto nextOrder = tickerSellBook.top();
                    nextOrder.quantity = bestSell.quantity - order.quantity;
                    tickerSellBook.pop();
                    tickerSellBook.push(nextOrder);
                    std::cout << order.clientName << " purchased " << order.quantity <<  " share of " << ticker << " from " << bestSell.clientName << " for $ " << bestSell.price << "/share" << std::endl;
                    return;
                }
                else if(order.quantity == bestSell.quantity) {
                    tickerSellBook.pop();
                    std::cout << order.clientName << " purchased " << order.quantity <<  " share of " << ticker << " from " << bestSell.clientName << " for $ " << bestSell.price << "/share" << std::endl;
                    return;
                }
                else {
                    order.quantity -= bestSell.quantity;
                    tickerSellBook.pop();
                    std::cout << order.clientName << " purchased " << order.quantity <<  " share of " << ticker << " from " << bestSell.clientName << " for $ " << bestSell.price << "/share" << std::endl;
                }
            }

            else {
                //no match at current buy price
                if(expiration != 0) orderBook.buyBooks[ticker].push(order); return;
            }
        }
        //empty book case
        if(expiration != 0) {
            orderBook.buyBooks[ticker].push(order);
            buyPrices[ticker][order.price] += order.quantity;
            std::cout << ticker << ' ' << order.price << ' ' << buyPrices[ticker][order.price] << '\n';
            try {
                boost::asio::write(socket, boost::asio::buffer(ticker + " " + std::to_string(order.price) + " " + std::to_string(buyPrices[ticker][order.price]) + "# "));
            } catch (const boost::system::system_error& e) {
                std::cerr << "Error occurred while writing to socket: " << e.what() << std::endl;
            }
        }
    }


    else {
        auto& tickerBuyBook = orderBook.buyBooks[ticker];
        while(!tickerBuyBook.empty()) {
            auto bestBuy = tickerBuyBook.top();
            if(bestBuy.expiration == -1 && (currentStamp - bestBuy.timeStamp < bestBuy.expiration)) {
                tickerBuyBook.pop(); break;
            }

            else if(order.price < bestBuy.price || order.price == bestBuy.price) {
                if(order.quantity < bestBuy.quantity) {
                    auto nextOrder = tickerBuyBook.top();
                    tickerBuyBook.pop();
                    nextOrder.quantity = bestBuy.quantity - order.quantity;
                    tickerBuyBook.push(nextOrder);
                    std::cout << bestBuy.clientName << " purchased " << order.quantity <<  " share of " << ticker << " from " << order.clientName << " for $ " << bestBuy.price << "/share" << std::endl;
                    return;
                }
                else if(order.quantity == bestBuy.quantity) {
                    tickerBuyBook.pop();
                    std::cout << bestBuy.clientName << " purchased " << order.quantity <<  " share of " << ticker << " from " << order.clientName << " for $ " << bestBuy.price << "/share" << std::endl;
                    return;
                }
                else {
                    order.quantity -= bestBuy.quantity;
                    tickerBuyBook.pop();
                    std::cout << bestBuy.clientName << " purchased " << order.quantity <<  " share of " << ticker << " from " << order.clientName << " for $ " << bestBuy.price << "/share" << std::endl;
                }
            }
            else {
                if(expiration != 0) orderBook.sellBooks[ticker].push(order); return;
            }
        }
        if(expiration != 0) orderBook.sellBooks[ticker].push(order); return;
    }
}

void MatchingEngine::orderDelete(int orderId) {

}

void MatchingEngine::orderUpdate(int orderId) {

}