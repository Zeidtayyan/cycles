#include "api.h"
#include "utils.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <queue>
#include <random>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

using namespace cycles;

class BotClient {
  Connection connection;
  std::string name;
  GameState state;
  Player my_player;
  std::mt19937 rng;

  bool is_valid_move(Direction direction) {
    // Check that the move does not overlap with any grid cell that is set to not 0
    auto new_pos = my_player.position + getDirectionVector(direction);
    if (!state.isInsideGrid(new_pos)) {
      return false;
    }
    if (state.getGridCell(new_pos) != 0) {
      return false;
    }
    return true;
  }

  int floodFill(const sf::Vector2i& start_pos, std::vector<std::vector<bool>>& visited) {
    std::queue<sf::Vector2i> queue;
    queue.push(start_pos);
    int area = 0;
    while (!queue.empty()) {
      sf::Vector2i pos = queue.front();
      queue.pop();
      if (!state.isInsideGrid(pos)) continue;
      if (visited[pos.x][pos.y]) continue;
      if (state.getGridCell(pos) != 0) continue;
      visited[pos.x][pos.y] = true;
      area++;
      // Add neighbors to queue
      for (int dir_value = 0; dir_value < 4; ++dir_value) {
        Direction dir = getDirectionFromValue(dir_value);
        sf::Vector2i next_pos = pos + getDirectionVector(dir);
        queue.push(next_pos);
      }
    }
    return area;
  }

  Direction decideMove() {
    std::vector<std::pair<Direction, int>> moves;
    const auto position = my_player.position;
    for (int dir_value = 0; dir_value < 4; ++dir_value) {
      Direction dir = getDirectionFromValue(dir_value);
      if (is_valid_move(dir)) {
        auto new_pos = position + getDirectionVector(dir);
        // Initialize visited matrix
        std::vector<std::vector<bool>> visited(
            state.gridWidth, std::vector<bool>(state.gridHeight, false));
        int area = floodFill(new_pos, visited);
        moves.emplace_back(dir, area);
        spdlog::debug("{}: Direction {} has area {}", name, dir_value, area);
      }
    }
    if (moves.empty()) {
      spdlog::error("{}: No valid moves available", name);
      exit(1);
    }
    // Choose the move(s) with maximum area
    int max_area = -1;
    std::vector<Direction> best_moves;
    for (const auto& move : moves) {
      if (move.second > max_area) {
        max_area = move.second;
        best_moves.clear();
        best_moves.push_back(move.first);
      } else if (move.second == max_area) {
        best_moves.push_back(move.first);
      }
    }
    // Randomly select among the best moves
    std::uniform_int_distribution<int> dist(0, static_cast<int>(best_moves.size()) - 1);
    Direction best_direction = best_moves[dist(rng)];
    spdlog::debug("{}: Selected direction {} with area {}", name,
                  getDirectionValue(best_direction), max_area);
    return best_direction;
  }

  void receiveGameState() {
    state = connection.receiveGameState();
    for (const auto& player : state.players) {
      if (player.name == name) {
        my_player = player;
        break;
      }
    }
  }

  void sendMove() {
    spdlog::debug("{}: Sending move", name);
    auto move = decideMove();
    connection.sendMove(move);
  }

 public:
  BotClient(const std::string& botName) : name(botName) {
    std::random_device rd;
    rng.seed(rd());
    connection.connect(name);
    if (!connection.isActive()) {
      spdlog::critical("{}: Connection failed", name);
      exit(1);
    }
  }

  void run() {
    while (connection.isActive()) {
      receiveGameState();
      sendMove();
    }
  }
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <bot_name>" << std::endl;
    return 1;
  }
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
  spdlog::set_level(spdlog::level::debug);
#endif
  std::string botName = argv[1];
  BotClient bot(botName);
  bot.run();
  return 0;
}