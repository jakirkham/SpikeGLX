% daqData = FetchLatestIm( myObj, NUM, channel_subset, downsample_ratio )
%
%     Get MxN matrix of the most recent stream data.
%     M = NUM = max samples to fetch.
%     N = channel count...
%     If channel_subset is not specified, N = all.
%
%     downsample_ratio is an integer (default = 1).
%
function [mat] = FetchLatestIm( s, num, varargin )

    if( nargin >= 3 )
        subset = varargin{1};
    else
        subset = GetSaveChansIm( s );
    end

    dwnsmp = 1;

    if( nargin >= 4 )

        dwnsmp = varargin{2};

        if ( ~isnumeric( dwnsmp ) || length( dwnsmp ) > 1 )
            error( 'Downsample factor must be a single numeric value' );
        end
    end

    scanCt = GetScanCountIm( s );

    if( num > scanCt )
        num = scanCt;
    end

    mat = FetchIm( s, scanCt-num, num, subset, dwnsmp );
end