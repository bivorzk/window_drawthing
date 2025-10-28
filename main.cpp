#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>
#include <windows.h>
#include "desktop_functions.h"

using namespace sf;
using namespace std;

// Function to create a thick line between two points
RectangleShape createThickLine(Vector2f point1, Vector2f point2, float thickness, Color color = Color::White) {
    RectangleShape line;
    
    // Calculate the distance between points
    Vector2f direction = point2 - point1;
    float length = sqrt(direction.x * direction.x + direction.y * direction.y);
    
    // Set line properties
    line.setSize(Vector2f(length, thickness));
    line.setFillColor(color);
    
    float angle = atan2(direction.y, direction.x) * 180.0f / M_PI;
    line.setRotation(sf::degrees(angle));
    
    // Set position to start point
    line.setPosition(point1);
    
    return line;
}

int main()
{

    int DESKTOP_X = 800;
    int DESKTOP_Y = 800;
    int CELL_SIZE = 10;
    float LINETHICKNESS = 5.0f;
    bool mousePressed = false;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);


    // create the window
    RenderWindow window(VideoMode({DESKTOP_X, DESKTOP_Y}), "My window");

    vector<Vector2f> currentStroke;  // Points for the current drawing stroke
    vector<RectangleShape> lines;
    
    // Desktop integration
    vector<DesktopIcon> desktopIcons;
    vector<CircleShape> iconDots;
    bool showDesktopIcons = false;

    // run the program as long as the window is open
    while (window.isOpen())
    {
        // check all the window's events that were triggered since the last iteration of the loop
        while (const std::optional event = window.pollEvent())
        {
            // "close requested" event: we close the window
            if (event->is<Event::Closed>())
                window.close();
                
            // Handle mouse button press
            if (event->is<Event::MouseButtonPressed>()) {
                if (event->getIf<Event::MouseButtonPressed>()->button == Mouse::Button::Left) {
                    mousePressed = true;
                    // Start a new stroke - clear the current stroke points
                    currentStroke.clear();
                }
            }
            
            // Handle mouse button release
            if (event->is<Event::MouseButtonReleased>()) {
                if (event->getIf<Event::MouseButtonReleased>()->button == Mouse::Button::Left) {
                    mousePressed = false;
                    // End the current stroke - clear the points so next stroke doesn't connect
                    currentStroke.clear();
                }
            }
            
            // Clear lines with right click
            if (event->is<Event::MouseButtonPressed>()) {
                if (event->getIf<Event::MouseButtonPressed>()->button == Mouse::Button::Right) {
                    currentStroke.clear();
                    lines.clear();
                }
            }
            
            // Handle keyboard input
            if (event->is<Event::KeyPressed>()) {
                if (event->getIf<Event::KeyPressed>()->code == Keyboard::Key::D) {
                    // Toggle desktop icons display
                    showDesktopIcons = !showDesktopIcons;
                    if (showDesktopIcons) {
                        // Get desktop icons
                        desktopIcons = GetDesktopIcons();
                        iconDots.clear();
                        cout << "Desktop icons displayed: " << desktopIcons.size() << " icons found" << endl;
                    } else {
                        iconDots.clear();
                        cout << "Desktop icons hidden" << endl;
                    }
                }
                
                // Move nearest desktop icon to mouse position on Space key
                if (event->getIf<Event::KeyPressed>()->code == Keyboard::Key::Space) {
                    cout << "Space key pressed!" << endl;
                    cout << "showDesktopIcons: " << showDesktopIcons << ", desktopIcons.size(): " << desktopIcons.size() << endl;
                    if (showDesktopIcons && !desktopIcons.empty()) {
                        cout << "Desktop icons are shown and available" << endl;
                        Vector2i mousePos = Mouse::getPosition(window);
                        int appX = static_cast<int>((static_cast<float>(mousePos.x) / DESKTOP_X) * screenWidth);
                        int appY = static_cast<int>((static_cast<float>(mousePos.y) / DESKTOP_Y) * screenHeight);
                        
                        cout << "Mouse in window: (" << mousePos.x << ", " << mousePos.y << ")" << endl;
                        cout << "Converted to desktop: (" << appX << ", " << appY << ")" << endl;
                        cout << "Screen size: " << screenWidth << "x" << screenHeight << ", Window size: " << DESKTOP_X << "x" << DESKTOP_Y << endl;
                        
                        // Find nearest icon
                        int nearestIndex = -1;
                        float minDist = numeric_limits<float>::max();
                        for (int i = 0; i < desktopIcons.size(); ++i) {
                            int dx = desktopIcons[i].position.x - appX;
                            int dy = desktopIcons[i].position.y - appY;
                            float dist = sqrt(dx * dx + dy * dy);
                            if (dist < minDist) {
                                minDist = dist;
                                nearestIndex = i;
                            }
                        }
                        
                        if (nearestIndex != -1) {
                            cout << "Nearest icon " << nearestIndex << " is currently at (" 
                                 << desktopIcons[nearestIndex].position.x << ", " << desktopIcons[nearestIndex].position.y << ")" << endl;
                            cout << "Distance: " << minDist << " pixels" << endl;
                            bool success = MoveDesktopIcon(nearestIndex, appX, appY);
                            cout << "Moving icon " << nearestIndex << " to (" << appX << ", " << appY << ") - " 
                                 << (success ? "Success" : "Failed") << endl;
                        }
                    }
                }
            }
        }

        if (window.hasFocus() && mousePressed) {
            Vector2i mousePos = Mouse::getPosition(window);
            Vector2f currentPoint(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
            
            // Add point if it's far enough from the last point (to avoid too many points)
            if (currentStroke.empty() || 
                (sqrt(pow(currentPoint.x - currentStroke.back().x, 2) + pow(currentPoint.y - currentStroke.back().y, 2)) > 3.0f)) {
                
                // If we have a previous point in the current stroke, create a line to connect them
                if (!currentStroke.empty()) {
                    RectangleShape line = createThickLine(currentStroke.back(), currentPoint, LINETHICKNESS);
                    lines.push_back(line);
                }
                
                currentStroke.push_back(currentPoint);
                cout << "Mouse Position: (" << mousePos.x << ", " << mousePos.y << ")\n";
                

            int mouseposX = mousePos.x;
            int mouseposY = mousePos.y;

            int appX = static_cast<int>((static_cast<float>(mouseposX) / DESKTOP_X) * screenWidth);
            int appY = static_cast<int>((static_cast<float>(mouseposY) / DESKTOP_Y) * screenHeight);


            int gridX = appX / CELL_SIZE;
            int gridY = appY / CELL_SIZE;

            cout << "App Coordinates: (" << appX << ", " << appY << ") - Grid Cell: (" << gridX << ", " << gridY << ")\n";

            }
        }
        
        // Clear the window
        window.clear();
        
        // Draw all the lines
        for (const auto& line : lines) {
            window.draw(line);
        }
        
        // Draw desktop icons if enabled
        if (showDesktopIcons) {
            for (const auto& dot : iconDots) {
                window.draw(dot);
            }
        }

        window.display();
    }
}