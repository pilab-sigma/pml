import numpy as np

def loadTxt(filename):
    X = np.loadtxt(filename)
    dim = int(X[0])
    size = []
    for i in range(dim):
        size.append(int(X[i+1]))
    X = np.reshape(X[dim+1:], size, order='F')
    return X


def saveTxt(filename, x, fmt='%.6f'):
    with open(filename, 'wb') as f:
        np.savetxt(f, np.append(x.ndim, x.shape), fmt='%d')
    with open(filename, 'ab') as f:
        temp = x.reshape(np.product(x.shape), order='F')
        np.savetxt(f, temp, fmt=fmt)
