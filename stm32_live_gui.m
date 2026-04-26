clc;
clear;



port = "COM4";
baud = 115200;


s = serialport(port, baud);
configureTerminator(s, "LF");
flush(s);

pause(2);
writeline(s, "s");   % Start streaming immediately


timeData = [];
u1Data = [];
u2Data = [];
encoder1Data = [];
encoder2Data = [];


isCleaned = false;


fig = figure( ...
    'Name', 'STM32 Encoder Stream', ...
    'NumberTitle', 'off', ...
    'MenuBar', 'none', ...
    'ToolBar', 'figure', ...
    'Color', 'w', ...
    'Position', [200 100 900 720]);

setappdata(fig, 'isRunning', true);
set(fig, 'CloseRequestFcn', @(src, event) closeFigure(src, s));

% ENCODER 1 
ax1 = axes('Parent', fig, 'Position', [0.08 0.74 0.88 0.18]);
grid(ax1, 'on');
hold(ax1, 'on');
xlabel(ax1, 'Time [s]');
ylabel(ax1, 'Encoder 1 [deg]');
title(ax1, 'Encoder 1');
lineEncoder1 = plot(ax1, nan, nan, 'DisplayName', 'Encoder 1');
legend(ax1, 'show', 'Location', 'northeast');

% ENCODER 2
ax2 = axes('Parent', fig, 'Position', [0.08 0.44 0.88 0.18]);
grid(ax2, 'on');
hold(ax2, 'on');
xlabel(ax2, 'Time [s]');
ylabel(ax2, 'Encoder 2 [deg]');
title(ax2, 'Encoder 2');
lineEncoder2 = plot(ax2, nan, nan, 'DisplayName', 'Encoder 2');
legend(ax2, 'show', 'Location', 'northeast');

%STATUS
statusText = uicontrol( ...
    'Style', 'text', ...
    'Parent', fig, ...
    'Units', 'normalized', ...
    'Position', [0.08 0.34 0.88 0.04], ...
    'String', 'Waiting for data...', ...
    'BackgroundColor', 'w', ...
    'HorizontalAlignment', 'left', ...
    'FontSize', 10);

% INPUT
uicontrol( ...
    'Style', 'text', ...
    'Parent', fig, ...
    'Units', 'normalized', ...
    'Position', [0.08 0.26 0.12 0.05], ...
    'String', 'Command:', ...
    'BackgroundColor', 'w', ...
    'HorizontalAlignment', 'left', ...
    'FontSize', 10);

cmdEdit = uicontrol( ...
    'Style', 'edit', ...
    'Parent', fig, ...
    'Units', 'normalized', ...
    'Position', [0.20 0.265 0.22 0.05], ...
    'String', '', ...
    'FontSize', 11, ...
    'Callback', @(src, event) sendCommand(src, s));

uicontrol( ...
    'Style', 'pushbutton', ...
    'Parent', fig, ...
    'Units', 'normalized', ...
    'Position', [0.44 0.265 0.12 0.05], ...
    'String', 'Send', ...
    'FontSize', 10, ...
    'Callback', @(src, event) sendCommand(cmdEdit, s));

uicontrol( ...
    'Style', 'text', ...
    'Parent', fig, ...
    'Units', 'normalized', ...
    'Position', [0.08 0.07 0.88 0.13], ...
    'String', sprintf(['Commands:\n', ...
                       '0..100 = motor power | + / - = adjust power | x = test 0..100\n', ...
                       'z = stop test | s = stream on | p = pause/stop | c = clear/reset']), ...
    'BackgroundColor', 'w', ...
    'HorizontalAlignment', 'left', ...
    'FontSize', 9);


while isvalid(fig) && getappdata(fig, 'isRunning')
    try
        if s.NumBytesAvailable > 0
            line = readline(s);
            values = sscanf(line, '%f,%f,%f,%f,%f');

            if numel(values) == 5
                t = values(1);
                u1 = values(2);
                u2 = values(3);
                encoder1 = values(4);
                encoder2 = values(5);

                timeData(end + 1) = t;
                u1Data(end + 1) = u1;
                u2Data(end + 1) = u2;
                encoder1Data(end + 1) = encoder1;
                encoder2Data(end + 1) = encoder2;

                set(lineEncoder1, 'XData', timeData, 'YData', encoder1Data);
                set(lineEncoder2, 'XData', timeData, 'YData', encoder2Data);

                statusText.String = sprintf( ...
                    't = %.4f s | u1 = %.2f | u2 = %.2f | Encoder 1 = %.2f deg | Encoder 2 = %.2f deg', ...
                    t, u1, u2, encoder1, encoder2);
            end
        end

        drawnow limitrate;

    catch err
        fprintf(2, 'Runtime error: %s\n', err.message);
        break;
    end
end


if ~isCleaned
    isCleaned = true;

    try
        if ~isempty(s)
            writeline(s, "p");
            pause(0.1);
        end
    catch
    end

    try
        delete(s);
    catch
    end
end


function sendCommand(cmdEdit, s)
    try
        cmd = strtrim(get(cmdEdit, 'String'));

        if isempty(cmd)
            return;
        end

        writeline(s, cmd);
        set(cmdEdit, 'String', '');

    catch err
        fprintf(2, 'Failed to send command: %s\n', err.message);
    end
end

function closeFigure(fig, s)
    try
        setappdata(fig, 'isRunning', false);
    catch
    end

    try
        if ~isempty(s)
            writeline(s, "p");
            pause(0.1);
        end
    catch
    end

    try
        delete(s);
    catch
    end

    if isvalid(fig)
        delete(fig);
    end
end
