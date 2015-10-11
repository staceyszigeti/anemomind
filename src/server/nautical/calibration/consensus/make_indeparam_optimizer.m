function out = make_indeparam_optimizer(A_, B_)
    [A, B] = compress_AB(A_, B_);
    out = @fun;
    function out = fun(X)
        out = zeros(size(X));
        for i = 1:numel(X),
            out(i) = optimize_param(A, B, X, i);
        end
    end
end

function out = optimize_param(A, B, X, index)
    [~, acols] = size(A);
    n = get_observation_count(A);
    F = kron(ones(n, 1), eye(2));
    Tx = normalize_vector(F(:, 1));
    Ty = normalize_vector(F(:, 2));
    P = A(:, index);
    Phat = (1/norm(P))*P;
    m = mask(acols, index);
    Q = A(:, m)*X(m) + B;
    Qhat = (1/norm(Q))*Q;
    K = [Tx Ty Phat Qhat];
    try
        v = calc_smallest_eigvec(K'*K);
    catch e,
        out = nan;
        return;
    end
    out = (v(3)/norm(P))/(v(4)/norm(Q));
end

function m = mask(n, index)
    m = true(n, 1);
    m(index) = false;
end