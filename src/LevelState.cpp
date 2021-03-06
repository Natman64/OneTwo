#include "LevelState.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include "Game.h"

one::LevelState::LevelState(Game* game)
    : game(game), gravityTimer(Game::GRAVITY_INTERVAL),
    fadeInTimer(Game::LEVEL_FADE_TIME), fadeOutTimer(Game::LEVEL_FADE_TIME),
    levelNumber(0), fromLevel(false)
{
    // initialize tile indices
    // TODO refactor this somewhere else

    hallTiles[ORANGE] = 36;
    hallTiles[BLUE] = 35;
    exitTiles[ORANGE] = 38;
    exitTiles[BLUE] = 37;

    gravityTimer.Start();
}

one::LevelState* one::LevelState::FromFile(Game* game, std::string path)
{
    std::map<std::string, Color> colorStrings;
    colorStrings["orange"] = ORANGE;
    colorStrings["blue"] = BLUE;

    // load a LevelState from a file

    LevelState* state = new LevelState(game); // empty level

    std::string levelText;
    std::vector<one::Line> textLines;
    std::map<Color, std::vector<one::Line> > hallLines;
    std::map<Color, Tile> entrances;
    std::map<Color, Tile> exits; 

    std::ifstream filestream;
    filestream.open(path);

    std::string line;
    while (getline(filestream, line))
    {
        // parse a line of the file

        std::cout << "Parsing line: " << line << std::endl;

        if (line.empty())
        {
            std::cout << "Skipping an empty line" << std::endl;
            continue; // don't parse empty lines
        }

        if (line[0] == '#')
        {
            std::cout << "Skipping a comment line" << std::endl;
            continue;
        }
        
        std::stringstream sstream(line);

        std::string word;

        sstream >> word; // read the first word

        if (!word.compare("text:"))
        {
            std::cout << "Found a text line" << std::endl;
            getline(sstream, levelText); // read the rest of the line
            std::cout << "Level text: " << levelText << std::endl;
        }

        if (!word.compare("word:"))
        {
            std::cout << "Found a label word line!" << std::endl;

            unsigned int x;
            unsigned int y;
            unsigned int l;

            std::string temp;

            sstream >> temp; // x
            x = atoi(temp.c_str());

            sstream >> temp; // y
            y = atoi(temp.c_str());

            sstream >> temp; // l
            l = atoi(temp.c_str());

            one::Line textLine(x, y, l, HORIZONTAL);

            textLines.push_back(textLine);

            std::cout << "Line: (" << textLine.x << ", " << textLine.y << ", " << textLine.length << ")" << std::endl;
        }

        if (!word.compare("hall:"))
        {
            sstream >> word;

            Color color = colorStrings[word];

            sstream >> word; // x
            unsigned int x = atoi(word.c_str());

            sstream >> word; // y
            unsigned int y = atoi(word.c_str());

            sstream >> word; // w
            unsigned int w = atoi(word.c_str());

            sstream >> word; // h
            unsigned int h = atoi(word.c_str());

            unsigned int l = w + h; // 0 will not affect this
            LineType type;

            if (w == 0)
            {
                std::cout << "Found a vertical line" << std::endl;
                type = VERTICAL;
            }
            if (h == 0)
            {
                std::cout << "Found a horizontal line" << std::endl;
                type = HORIZONTAL;
            }

            hallLines[color].push_back(Line(x, y, l, type));
        }

        if (!word.compare("entrance:"))
        {
            sstream >> word; // color

            Color color = colorStrings[word]; // parse color enum

            sstream >> word; // x
            int x = atoi(word.c_str());

            sstream >> word; // y
            int y = atoi(word.c_str());

            entrances[color] = Tile(x, y);            
        }

        if (!word.compare("exit:"))
        {
            sstream >> word; // color

            Color color = colorStrings[word];

            sstream >> word; // x
            int x = atoi(word.c_str());

            sstream >> word; // y
            int y = atoi(word.c_str());

            exits[color] = Tile(x, y);            
        }

        if (!word.compare("tutorial:"))
        {
            // TODO handle tutorials
        }
    }

    std::cout << "Got to this point" << std::endl;

    // create label
    state->label = new Label(levelText, textLines);
    state->level.hallLines = hallLines;
    state->level.exits = exits;
    state->entrances = entrances;

    std::cout << "Got to this point" << std::endl;

    // create players
    for (Color color = COLOR_BEGIN; color != COLOR_END; color = (Color)((int)color + 1))
    {
        state->players[color] = Player(color, entrances[color].x * Game::TILE_SIZE, entrances[color].y * Game::TILE_SIZE);
    }

    filestream.close();

    state->fadeInTimer.Start();

    std::cout << "Got to this point" << std::endl;

    return state;
}

one::LevelState* one::LevelState::FromLevel(Game* game, int level)
{
    std::ostringstream pathStream;

    pathStream << "assets/levels/";

    if (level < 10) // 1 digit
    {
        pathStream << "0"; // pad with a 0
    }

    pathStream << level << ".one";

    std::cout << pathStream.str() << std::endl;

    LevelState* state = LevelState::FromFile(game, pathStream.str());

    state->levelNumber = level;
    state->fromLevel = true;

    return state;
}

void one::LevelState::Update(unsigned int deltaMS, one::Input& input)
{
    // update the fade timers
    fadeInTimer.Update(deltaMS);
    fadeOutTimer.Update(deltaMS);

    if (fadeOutTimer.IsStarted())
    {
        if (fadeOutTimer.IntervalPassed())
        {
            fadeOutTimer.Stop();

            game->SetState(LevelState::FromLevel(game, levelNumber + 1));
        }

        // don't update anything else! The level has been beaten and bad things will happen!
        return;
    }

    updateGravity(deltaMS);

    bool allWillMove = true;
    for (Color color = COLOR_BEGIN; color != COLOR_END; color = (Color)((int)color + 1))
    {
        if (!players[color].WillMove(deltaMS, input, level))
        {
            allWillMove = false;
        }
    }

    if (allWillMove)
    {
        for (Color color = COLOR_BEGIN; color != COLOR_END; color = (Color)((int)color + 1))
        {
            players[color].Update(deltaMS, input, level);
        }
    }

    bool allOnExits = true;

    for (Color color = COLOR_BEGIN; color != COLOR_END; color = (Color)((int)color + 1))
    {
        if (!players[color].IsOnExit(level))
        {
            allOnExits = false;
            break;
        }
    }

    if (allOnExits)
    {
        if (!fadeOutTimer.IsStarted())
        {
            fadeOutTimer.Start();
        }
    }

    if (input.IsKeyPressed(SDLK_r))
    {
        resetLevel();
    }
}

void one::LevelState::Draw(one::Graphics& graphics)
{
    label->Draw(graphics);

    for (Color color = COLOR_BEGIN; color != COLOR_END; color = (Color)((int)color + 1))
    {
        drawHalls(color, graphics);

        drawExit(color, graphics);
    }

    for (Color color = COLOR_BEGIN; color != COLOR_END; color = (Color)((int)color + 1))
    {
        players[color].Draw(graphics);
    }

    drawFade(graphics);
}

void one::LevelState::drawFade(Graphics& graphics)
{
    if (fadeInTimer.IsStarted())
    {
        // is fading in
        if (fadeInTimer.IntervalPassed())
        {
            // done fading
            fadeInTimer.Stop();
        }
        else
        {
            graphics.DrawFade(1.0f - fadeInTimer.IntervalProgress());
        }
    }
    else if (fadeOutTimer.IsStarted())
    {
        // is fading out
        if (fadeOutTimer.IntervalPassed())
        {
            // done fading
            fadeOutTimer.Stop();
        }
        else
        {
            graphics.DrawFade(fadeOutTimer.IntervalProgress());
        }
    }
}

void one::LevelState::resetLevel()
{
    for (Color color = COLOR_BEGIN; color != COLOR_END; color = (Color)((int)color + 1))
    {
        players[color].SetPosition(entrances[color].x * Game::TILE_SIZE, entrances[color].y * Game::TILE_SIZE);
    }

    gravityTimer.Reset();
}

void one::LevelState::updateGravity(unsigned int deltaMS)
{
    gravityTimer.Update(deltaMS);

    if (gravityTimer.IntervalPassed())
    {
        // pull players towards their nearest tiles

        for (Color color = COLOR_BEGIN; color != COLOR_END; color = (Color)((int)color + 1))
        {
            Player* player = &players[color];

            int x = player->GetX();
            int y = player->GetY();

            // retrieve tile location in PIXEL coordinates
            Tile tile = player->GetNearestTile();
            int tileX = tile.x * Game::TILE_SIZE;
            int tileY = tile.y * Game::TILE_SIZE;

            if (x != tileX)
            {
                int xMovement = (tileX - x) / abs(tileX - x); // move 1 at a time
                x += xMovement;
            }

            if (y != tileY)
            {
                int yMovement = (tileY - y) / abs(tileY - y);
                y += yMovement;
            }

            player->SetPosition(x, y);
        }
    }
}

void one::LevelState::drawHalls(one::Color color, one::Graphics& graphics)
{
    std::vector<one::Line> lines;
    std::vector<one::Line>::iterator it;

    lines.assign(level.hallLines[color].begin(), level.hallLines[color].end());
    it = lines.begin();

    while (it != lines.end())
    {
        one::Line line = *it;

        int x = line.x;
        int y = line.y;

        while ((line.type == HORIZONTAL && x < line.x + line.length) ||
                (line.type == VERTICAL && y < line.y + line.length))
        {
            graphics.DrawSprite("tiles", hallTiles[color], x * Game::TILE_SIZE, y * Game::TILE_SIZE);

            if (line.type == HORIZONTAL) ++x;
            if (line.type == VERTICAL) ++y;
        }

        ++it;
    }
}

void one::LevelState::drawExit(one::Color color, one::Graphics& graphics)
{
    graphics.DrawSprite("tiles", exitTiles[color], level.exits[color].x * Game::TILE_SIZE,
            level.exits[color].y * Game::TILE_SIZE);
}
